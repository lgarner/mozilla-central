#ifndef _NDEFRECORD_H
#define _NDEFRECORD_H

#include "nsIDOMMozNdefRecord.h"
#include "nsString.h"
#include "jsapi.h"

namespace mozilla {
namespace dom {
namespace nfc {

/**
 *
 */
class NdefRecord MOZ_FINAL : public nsIDOMMozNdefRecord
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZNDEFRECORD

  NdefRecord();
  static nsresult NewNdefRecord(nsISupports** aRecord);
#if 0
  NdefRecord(const char tnf, const nsAString& type, const nsAString& id, const jsval& payload);
  static nsresult Create(const char tnf, const nsAString& type, const nsAString& id, const jsval& payload,
                         JSContext*aCx, nsIDOMMozNdefRecord** aRecord);
   // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* aContext,
                        JSObject* aObject, PRUint32 aArgc, jsval* aArgv);
#endif
 
  virtual ~NdefRecord() {}

  char tnf;
  nsString type;
  nsString id;
  jsval payload;
};

} // namespace nfc
} // namespace dom
} // namespace mozilla

#endif /* _NDEFRECORD_H */
