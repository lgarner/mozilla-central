/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NdefRecord.h"
#include "nsIDOMClassInfo.h"

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
  tnf = 0;
  payload = JSVAL_NULL;
}

nsresult NdefRecord::NewNdefRecord(nsISupports* *aRecord)
{
  *aRecord = new NdefRecord();
  return NS_OK;
}

/********************************
 * NS_METHODIMPs
 ********************************/
NS_IMETHODIMP
NdefRecord::GetTnf(char* aTnf)
{
  *aTnf = tnf;
  return NS_OK;
}

NS_IMETHODIMP
NdefRecord::GetType(nsAString& aType)
{
  aType = type;
  return NS_OK;
}

NS_IMETHODIMP
NdefRecord::GetId(nsAString& aId)
{
  aId = id;
  return NS_OK;
}

NS_IMETHODIMP
NdefRecord::GetPayload(JS::Value* aPayload)
{
  *aPayload = payload;
  return NS_OK;
}

} // namespace nfc
} // namespace dom
} // namespace mozilla
