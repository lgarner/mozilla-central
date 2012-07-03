/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NdefRecord.h"
#include "nsIDOMClassInfo.h"
#include "nsString.h"
#include "jsapi.h"

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

#if 0

NdefRecord::NdefRecord(const char tnf, const nsAString& type, const nsAString& id, const jsval& payload)
  : tnf(tnf),
    type(type),
    id(id),
    payload(payload)
{}


/* static */ nsresult
NdefRecord::Create(const char tnf, const nsAString& type, const nsAString& id, const jsval& payload,
                   JSContext*aCx, nsIDOMMozNdefRecord** aRecord) {
  *aRecord = new NdefRecord(tnf, type, id, payload); 
  return NS_OK; 
}
#endif

nsresult NewNdefRecord(nsISupports** aRecord)
{
  *aRecord = new NdefRecord();
  return NS_OK;
}

#if 0
//-----------------------------------------------------------------------------
// NdefRecord::nsIJSNativeInitializer methods:
//-----------------------------------------------------------------------------
NS_IMETHODIMP
NdefRecord::Initialize(nsISupports* aOwner,
                        JSContext* aContext,
                        JSObject* aObject,
                        PRUint32 aArgc,
                        JS::Value* aArgv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  // Parameter marshalling:
  int tnf;

  if (aArgc < 4) {
    NS_ABORT_IF_FALSE(JS_FALSE, "NdefRecord: Not enough arguments in constructor");
    return NS_ERROR_INVALID_ARG;
  }
  nsresult result;
  JSObject* jsobj;
  result = JS_ValueToInt32(aContext, aArgv[0], &tnf);
  JSString *jsstrType = JS_ValueToString(aContext, aArgv[1]);
  JSString *jsstrId = JS_ValueToString(aContext, aArgv[2]);
  result = JS_ValueToObject(aContext, aArgv[3], &jsobj);
#if 0 
  if (jsobj == JSVAL_NULL) {
    NS_ABORT_IF_FALSE(JS_FALSE, "NdefRecord: Constructor payload is empty");
    return NS_ERROR_INVALID_ARG; // No playload.
  } 
#endif
  return NS_OK; 
}

#endif

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
