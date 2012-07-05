#ifndef _NDEFRECORD_H
#define _NDEFRECORD_H

#include "mozilla/Attributes.h"
#include "nsString.h"
#include "jsapi.h"
#include "nsIDOMMozNdefRecord.h"

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
  static nsresult NewNdefRecord(nsISupports* *aNewRecord);
#if 0
  NdefRecord(const char tnf, const nsAString& type, const nsAString& id, const jsval& payload);
  static nsresult Create(const char tnf, const nsAString& type, const nsAString& id, const jsval& payload,
                         JSContext*aCx, nsIDOMMozNdefRecord** aRecord);
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
