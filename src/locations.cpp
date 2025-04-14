/*

  Copyright [2025] [Leonardo Julca]

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#include "locations.h"

using namespace std;

//
// ServiceUser
//

ServiceUser::ServiceUser()
 : serviceType(SERVICE_TYPE_NONE)
{
};

ServiceUser::ServiceUser(const ServiceUser& otherService)
 : serviceType(otherService.serviceType),
   servicePtr(otherService.servicePtr.lock()),
   userName(otherService.userName)
{
};

ServiceUser::ServiceUser(uint8_t serviceType, string nUserName)
 : serviceType(SERVICE_TYPE_NONE),
   userName(nUserName)
{
};

ServiceUser::ServiceUser(uint8_t serviceType, string nUserName, shared_ptr<void> nServicePtr)
 : serviceType(SERVICE_TYPE_NONE),
   servicePtr(nServicePtr),
   userName(nUserName)
{
};

void ServiceUser::Reset()
{
  serviceType = SERVICE_TYPE_NONE;
  servicePtr.reset();
  userName.clear();
}

ServiceUser::~ServiceUser()
{
}
