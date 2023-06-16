/*--- START ---

Notes on using GstPushSrc
-------------------------

GstPushSrc has 4 virtual functions: 'create', 'alloc', 'fill' and 'query'. The
default for 'create' is to call 'alloc' to allocate a buffer and 'fill' to
fill it with data. I'm going to override alloc (because the default fails to
allocate buffers of the size I need) and fill, because you HAVE to, as it
actually fills the buffer with a frame of data. 'query' intercepts any
incoming QUERY events to the element, but for now we're happy with the default
handling of those events.

In general we want to override the most-derived routines that we can in order
to do our work, leaving as much processing as possible to the default element
code. Thus we override the alloc and fill routines in the GstPushSrcClass,
while overriding other elements in the parent GstBaseSrcClass, as needed.

GstPushSrc is a subclass of GstBaseSrc

GstBaseSrcClass has a large number of virtual functions, and I've yet to
figure out what most of them are actually for (the documentation leaves much
to be desired.) As far as I can tell, we need to override several, but the
list keeps changing as I find out new things about the model of how an element
works (something else that should be explicitly documented somewhere, but
isn't). This is my current best guess at what is what.

GstBaseSrc Virtual Methods:

* get_caps - get caps from subclass

get_caps is called by BaseSrc in response to receiving a GST_QUERY_CAPS. The
result we return is again returned as the result of the query. We start with
the basic CAPS from our source pad template, and then fixate any fields that
the user has requested specific values for.

* negotiate - decide on caps

This routine implements standard caps negotiations. Since we don't have any
special caps that need taking care of, we went with the default implementation.

* fixate - called if, in negotiation, caps need fixating

fixate is called if the pipeline has agreed on a common set of caps, but it
still has ranges and options in it. Our job is to set each range and option to
our preferred value, so the caps are fixed, and return the result. Note that
by this point any user preferred settings have already been negotiated so all
we have left to do is set values to our defaults.

* set_caps - notify the subclass of new caps

set_caps is called by BaseSrc after negotiation has arrived at fixed
caps. These negotiated caps need to be acknowledged and remembered. I believe
we return true if we are able to accept an end to negotiation at this time, or
false if later negotiation will be required.

NOTE: We would only ever return false if we had been negotiating with
preliminary caps because we're waiting for information from a source device
in order to provide our final caps, and we hadn't yet done that. Since we
have no source device to talk to, this never happens in our case.

* decide_allocation - setup allocation query

I believe this routine negotiates which allocation system and/or buffer pool
is going to be used as the source of buffers for this element. Not knowing
anything about how this works, I'm just using the default.

* start - start processing
* stop  - stop processing

These methods are called to mark the start and end of frame output from the
source. 'start' usually does such tasks as opening devices for reading, and
resetting the clock, while 'stop' does such tasks as cleaning up after 'start'
(such as closing devices and freeing data structures allocated by 'start'.)

* get_times - return start and stop times when a buffer should be pushed.

This is a very confusing method as its description here would suggest that its
job is to simply generate the correct timestamp and duration for the current
buffer and return them, but every example in the source I can find does
something far more complex. Often, implementations of this routine read these
values out of the very buffer they are supposed to be setting values for...

For now we just naively generate a buffer duration equal to the reciprocal of
the frame rate, and generate each new timestamp by adding that duration to the
last timestamp. This *seems* to work.

* get_size - get the total size of the resource in bytes

This routine is supposed to return the size, in the set GstFormat for the
source, of the entire source output. I'm assuming this routine is never
actually called for live sources, so I've just left it at the default.

* is_seekable - check if the resource is seekable

Simply returns TRUE or FALSE depending on if the source is random access or not.

* prepare_seek_segment - Prepare to perform do_seek(), using current GstFormat.

Not entirely sure what this does, but as I'm not implementing a seekable
source, I left this as its default.

* do_seek - notify subclasses of a seek

I think this actually does a seek in the source stream, but as I am not
implementing a seekable source, I've left this at the default.

* unlock - unlock any pending access to the resource

Text for this method says "subclasses should unlock any function ASAP". Not
really clear what is required, but as I lock access to my frame source during
a fairly lengthy rendering process, I interpreted calls on this method to mean
I should abort the rendering and unlock the source.

* unlock_stop - Clear any pending unlock request, as we succeeded in unlocking

Again, not really sure what this is all about, as it seems to imply that it
may NOT be necessary to actually unlock when asked to unlock. In any case, we
currently leave this method at its default. Once you ask us to unlock, we
unlock.

* query - notify subclasses of a query

This method allows the subclass to override the standard handling of one or
more types of incoming queries. We're happy with the default query handling,
so we left this alone.

* event - notify subclasses of an event

This method allows the subclass to override the standard handling of one or
more types of event. In our case we are happy with everything but navigation
events, which we have to handle ourselves. So we check for navigation events
here and handle them if so (returning TRUE to say its taken care of). For
other events we just call into our superclass's event method and return
whatever it returns. ie:

   if( GST_EVENT_TYPE(event) != GST_EVENT_NAVIGATION )
     return GST_BASE_SRC_CLASS(gst_mysrc_parent_class)->event(base,event);

* create - ask the subclass to create a buffer with offset and size.

The default implementation of this method will call the alloc and fill methods
listed below, so typically one either overrides this method, or overrides one
or both of alloc and fill, but not all three.

* alloc - ask the subclass to allocate an empty output buffer.

The default implementation will use the negotiated allocator, however I found
that it was allocating *empty* buffers that I couldn't store frames in. No
idea why. In the end I overrode this routine with one that checks for a custom
buffer pool or custom allocator and uses them if found. It allocates empty
buffers of the correct size for storing video frames in the current format. I
made extensive use of the gst_video_ routines to manage this.

* fill - ask the subclass to fill the buffer with data from offset and size

As I'm implementing a live source, the offset and size parameters are to be
ignored, and I simply render the next frame of video output into the given
buffer.

--- END ---*/

#ifndef _GST_VIDEOCORE_HT_CONNECTOR_H_
#define _GST_VIDEOCORE_HT_CONNECTOR_H_

#include <gst/base/gstpushsrc.h>

#include "packet.h"
#include "hqueue.h"

#define MTU 1500

extern HQUEUE packetQueue;

G_BEGIN_DECLS

#define GST_TYPE_VIDEOCORE_HT_CONNECTOR   (gst_videocore_ht_connector_get_type())
#define GST_VIDEOCORE_HT_CONNECTOR(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIDEOCORE_HT_CONNECTOR,GstVideocoreHtConnector))
#define GST_VIDEOCORE_HT_CONNECTOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIDEOCORE_HT_CONNECTOR,GstVideocoreHtConnectorClass))
#define GST_IS_VIDEOCORE_HT_CONNECTOR(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIDEOCORE_HT_CONNECTOR))
#define GST_IS_VIDEOCORE_HT_CONNECTOR_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIDEOCORE_HT_CONNECTOR))

typedef struct _GstVideocoreHtConnector GstVideocoreHtConnector;
typedef struct _GstVideocoreHtConnectorClass GstVideocoreHtConnectorClass;

struct _GstVideocoreHtConnector
{
    GstPushSrc base_videocorehtconnector;
    GstBuffer* current_buffer;
};

struct _GstVideocoreHtConnectorClass
{
    GstPushSrcClass base_videocorehtconnector_class;
};


GType gst_videocore_ht_connector_get_type (void);

GstVideocoreHtConnector* gst_videocore_ht_connector_new();

G_END_DECLS

#endif
