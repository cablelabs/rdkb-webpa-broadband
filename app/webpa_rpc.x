/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

struct webpa_topic
{
  string topic<>;
};

struct webpa_register_request
{
  int         prog_num;
  int         prog_vers;
  string      proto<>;
  string      host<>;
  webpa_topic topics<>;
};

struct webpa_register_response
{
  int id;
};

struct webpa_send_message_request
{
  opaque data<>;
};

struct webpa_send_message_response
{
  int ack;
};

program WEBPA_PROG
{
  version WEBPA_VERS
  {
    webpa_register_response     WEBPA_REGISTER(webpa_register_request)          = 1;
    webpa_send_message_response WEBPA_SEND_MESSAGE(webpa_send_message_request)  = 2;
  } = 1;
} = 0x20000008;
