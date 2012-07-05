/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NdefRecord.h"
#include "nsIDOMClassInfo.h"

#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk", args)
#else
#define LOG(args...) 
#endif

DOMCI_DATA(MozNdefRecord, mozilla::dom::nfc::NdefRecord)

namespace mozilla {
namespace dom {
namespace nfc {

// For Query interface to find the mapped interface.
NS_INTERFACE_MAP_BEGIN(NdefRecord)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozNdefRecord)
  NS_INTERFACE_MAP_ENTRY(nsISupports) 
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozNdefRecord)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(NdefRecord)
NS_IMPL_RELEASE(NdefRecord)

/********************************
 * Constructors/Destructors
 ********************************/

NdefRecord::NdefRecord()
{
  LOG("DOM NdefRecord: No parameter instantiation");
  tnf = 0;
  payload = JSVAL_NULL;
}

nsresult NdefRecord::NewNdefRecord(nsISupports* *aNewObject)
{
  LOG("DOM nsNdefRecord: No parameter instantiation");
  NS_ADDREF(*aNewObject = new NdefRecord());
  return NS_OK;
}

/************************************
 * NS_METHODIMPs (Getter and Setter)
 ************************************/
NS_IMETHODIMP
NdefRecord::GetTnf(char* aTnf)
{
  LOG("DOM NdefRecord.GetTnf");
  *aTnf = tnf;
  return NS_OK;
}

NS_IMETHODIMP
NdefRecord::SetTnf(const char aTnf)
{
  LOG("DOM NdefRecord.SetTnf");
  tnf = aTnf;
  return NS_OK; 
}

NS_IMETHODIMP
NdefRecord::GetType(nsAString& aType)
{
  LOG("DOM NdefRecord.GetType");
  aType = type;
  return NS_OK;
}

NS_IMETHODIMP
NdefRecord::SetType(const nsAString & aType)
{
  LOG("DOM NdefRecord.SetType");
  type = aType;
  return NS_OK;
}

NS_IMETHODIMP
NdefRecord::GetId(nsAString& aId)
{
  LOG("DOM NdefRecord.GetId");
  aId = id;
  return NS_OK;
}

NS_IMETHODIMP
NdefRecord::SetId(const nsAString& aId)
{
  LOG("DOM NdefRecord.SetId");
  id = aId;
  return NS_OK;
}

NS_IMETHODIMP
NdefRecord::GetPayload(JS::Value* aPayload)
{
  LOG("DOM NdefRecord.GetPayload");
  *aPayload = payload;
  return NS_OK;
}

NS_IMETHODIMP
NdefRecord::SetPayload(const JS::Value& aPlayload)
{
  LOG("DOM NdefRecord.SetPayload");
  payload = aPlayload;
  return NS_OK;
}

} // namespace nfc
} // namespace dom
} // namespace mozilla
