##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2017 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
lib_LTLIBRARIES=libwal.la
libwal_la_SOURCES=wal.c ssp_messagebus_interface.c ssp_main.c ssp_action.c cosa_Webpa_dml.c plugin_main.c
libwal_la_CFLAGS = -I$(top_srcdir)/wal/include $(DBUS_CFLAGS)
libwal_la_LDFLAGS = -lccsp_common -lm -lpthread -llog4c -ltrower-base64
