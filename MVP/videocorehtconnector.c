#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include "videocorehtconnector.h"

GST_DEBUG_CATEGORY_STATIC (gst_videocore_ht_connector_debug_category);
#define GST_CAT_DEFAULT gst_videocore_ht_connector_debug_category

/* prototypes */
static void gst_videocore_ht_connector_finalize (GObject * object);

static gboolean gst_videocore_ht_connector_negotiate (GstBaseSrc * src);
static gboolean gst_videocore_ht_connector_start (GstBaseSrc * src);
static gboolean gst_videocore_ht_connector_stop (GstBaseSrc * src);
static gboolean gst_videocore_ht_connector_query (GstBaseSrc * src, GstQuery * query);
static GstFlowReturn gst_videocore_ht_connector_create (GstBaseSrc * src, guint64 offset, guint size, GstBuffer ** buf);
static gboolean gst_videocore_ht_connector_is_seekable(GstBaseSrc * src);

enum
{
    PROP_0
};

/* pad templates */
static GstStaticPadTemplate gst_videocore_ht_connector_src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-h264"));

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (
        GstVideocoreHtConnector, gst_videocore_ht_connector, GST_TYPE_PUSH_SRC,
        GST_DEBUG_CATEGORY_INIT (gst_videocore_ht_connector_debug_category, "videocorehtconnector", 0,
                                 "debug category for videocorehtconnector element"));

static void gst_videocore_ht_connector_class_init (GstVideocoreHtConnectorClass * klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

    gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
                                               &gst_videocore_ht_connector_src_template);

    gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
                                           "Happytime video connector", "Generic",
                                           "Recieve video packet from HappyTime client, copy it and wrap for gstreamer",
                                           "Nikolay Morozov <morozov@kraftway.ru>");

    gobject_class->finalize = gst_videocore_ht_connector_finalize;
    base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_videocore_ht_connector_negotiate);
    base_src_class->start = GST_DEBUG_FUNCPTR (gst_videocore_ht_connector_start);
    base_src_class->stop = GST_DEBUG_FUNCPTR (gst_videocore_ht_connector_stop);
    base_src_class->query = GST_DEBUG_FUNCPTR (gst_videocore_ht_connector_query);
    base_src_class->create = GST_DEBUG_FUNCPTR (gst_videocore_ht_connector_create);
    base_src_class->is_seekable=GST_DEBUG_FUNCPTR(gst_videocore_ht_connector_is_seekable);
}

static void gst_videocore_ht_connector_init (GstVideocoreHtConnector *videocorehtconnector)
{
}

static gboolean gst_videocore_ht_connector_is_seekable(GstBaseSrc * src)
{
    return FALSE;
}

void gst_videocore_ht_connector_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
    GstVideocoreHtConnector *videocorehtconnector = GST_VIDEOCORE_HT_CONNECTOR (object);

    GST_DEBUG_OBJECT (videocorehtconnector, "get_property");

    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

void gst_videocore_ht_connector_finalize (GObject * object)
{
    GstVideocoreHtConnector *videocorehtconnector = GST_VIDEOCORE_HT_CONNECTOR (object);

    GST_DEBUG_OBJECT (videocorehtconnector, "finalize");

    G_OBJECT_CLASS (gst_videocore_ht_connector_parent_class)->finalize (object);
}

/* decide on caps */
static gboolean gst_videocore_ht_connector_negotiate (GstBaseSrc * src)
{
    GstVideocoreHtConnector *videocorehtconnector = GST_VIDEOCORE_HT_CONNECTOR (src);

    GST_DEBUG_OBJECT (videocorehtconnector, "negotiate");

    return TRUE;
}

static gboolean gst_videocore_ht_connector_start (GstBaseSrc * src)
{
    GstVideocoreHtConnector *videocorehtconnector = GST_VIDEOCORE_HT_CONNECTOR (src);

    GST_DEBUG_OBJECT (videocorehtconnector, "start");

    return TRUE;
}

static gboolean gst_videocore_ht_connector_stop (GstBaseSrc * src)
{
    GstVideocoreHtConnector *videocorehtconnector = GST_VIDEOCORE_HT_CONNECTOR (src);

    GST_DEBUG_OBJECT (videocorehtconnector, "stop");

    return TRUE;
}

/* notify subclasses of a query */
static gboolean gst_videocore_ht_connector_query (GstBaseSrc * src, GstQuery * query)
{
    GstVideocoreHtConnector *videocorehtconnector = GST_VIDEOCORE_HT_CONNECTOR (src);

    GST_DEBUG_OBJECT (videocorehtconnector, "query");

    return GST_BASE_SRC_CLASS(gst_videocore_ht_connector_parent_class)->query(src,query);
}

/* ask the subclass to create a buffer with offset and size */
static GstFlowReturn gst_videocore_ht_connector_create (GstBaseSrc * src, guint64 offset, guint size, GstBuffer ** buf)
{
    GstVideocoreHtConnector *videocorehtconnector = GST_VIDEOCORE_HT_CONNECTOR (src);

    GST_DEBUG_OBJECT (videocorehtconnector, "create");

    Packet p;
    *buf=gst_buffer_new();

    videocorehtconnector->current_buffer=NULL;

    while(!videocorehtconnector->current_buffer)
    {
        if(!hqBufGet(&packetQueue,(char*)&p)) return  GST_FLOW_ERROR;

        GstMemory* memory = gst_memory_new_wrapped((GstMemoryFlags)(0), p.data, p.len, 0, p.len, p.data, g_free);
        gst_buffer_insert_memory(videocorehtconnector->current_buffer, -1, memory);
    }
    *buf=videocorehtconnector->current_buffer;

    return GST_FLOW_OK;
}

static gboolean plugin_init (GstPlugin * plugin)
{
    return gst_element_register (plugin, "videocorehtconnector", GST_RANK_NONE, GST_TYPE_VIDEOCORE_HT_CONNECTOR);
}

GstVideocoreHtConnector *gst_videocore_ht_connector_new()
{
    return g_object_new(GST_TYPE_VIDEOCORE_HT_CONNECTOR, NULL);
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "happytimeconnector"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "happytimeconnector"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://git.jetbrains.space/horizon-dev/video-core/MVP.git"
#endif


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, videocorehtconnector,
                   "Videocore HappyTime connector", plugin_init, VERSION,
                   "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

