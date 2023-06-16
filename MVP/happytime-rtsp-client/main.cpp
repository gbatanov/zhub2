/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2020, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#include "RtspClient.h"
#include <QtWidgets/QApplication>
#include "rtsp_parse.h"
#include "http_parse.h"


#ifdef _DEBUG
static void av_log_callback(void* ptr, int level, const char* fmt, va_list vl)   
{
    int htlv = HT_LOG_INFO;
	char buff[4096];

	if (AV_LOG_QUIET == level)
	{
	    return;
	}
	
	vsnprintf(buff, sizeof(buff), fmt, vl);

    if (AV_LOG_TRACE == level || AV_LOG_VERBOSE == level)
    {
        htlv = HT_LOG_TRC;
    }
	else if (AV_LOG_DEBUG == level)
	{
	    htlv = HT_LOG_DBG;
	}
	else if (AV_LOG_INFO == level)
    {
        htlv = HT_LOG_INFO;
    }
    else if (AV_LOG_WARNING == level)
    {
        htlv = HT_LOG_WARN;
    }
    else if (AV_LOG_ERROR == level)
    {
        htlv = HT_LOG_ERR;
    }
    else if (AV_LOG_FATAL == level || AV_LOG_PANIC == level)
    {
        htlv = HT_LOG_FATAL;
    }
	
	log_print(htlv, "%s", buff);
}
#endif

void setAppStyleSheet(QApplication & app)
{
    QFile file(QCoreApplication::applicationDirPath() + "/RtspClient.css");
	
    file.open(QFile::ReadOnly);
    
	app.setStyleSheet(QLatin1String(file.readAll()));
}

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());
	QCoreApplication::setOrganizationName("happytimesoft");
	
    setAppStyleSheet(a);
    
    network_init();
	
#ifdef _DEBUG
	av_log_set_callback(av_log_callback);
	av_log_set_level(AV_LOG_QUIET);
#endif

	sys_buf_init(200);
    rtsp_parse_buf_init(200);
    http_msg_buf_init(200);
    
	RtspClient w(0, Qt::WindowSystemMenuHint|Qt::WindowMinMaxButtonsHint|Qt::WindowCloseButtonHint);

	w.showMaximized();
	
	return a.exec();
}


