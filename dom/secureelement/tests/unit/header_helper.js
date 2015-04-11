/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Strips spaces, and returns a byte array.
 */
function formatHexAndCreateByteArray(hexStr) {
  return SEUtils.hexStringToByteArray(hexStr.replace(/\s+/g, ""));
}

/**
 * These constants below should ideally be a superset of constants that
 * GPAccessRulesManager.js uses.
 */

/* Object Directory File (ODF) is an elementary file which contain
   pointers to other EFs. It is specified in PKCS#15 section 6.7. */
const ODF_DF = [0x50, 0x31];

/* ISO 7816-4: secure messaging */
const CLA_SM = 0x00;

/* ISO 7816-4, 5.4.1 table 11 */
const INS_SF = 0xA4; // select file
const INS_GR = 0xC0; // get response
const INS_RB = 0xB0; // read binary

/* ISO 7816-4: select file, see 6.11.3, table 58 & 59 */
const P1_SF_DF = 0x00; // select DF
const P2_SF_FCP = 0x04; // return FCP

/* ISO 7816-4: read binary, 6.1.3. P1 and P2 describe offset of the first byte
   to be read. We always read the whole files at the moment. */
const P1_RB = 0x00;
const P2_RB = 0x00;

/* ISO 7816-4: get response, 7.1.3 table 74,  P1-P2 '0000' (other values RFU) */
const P1_GR = 0x00;
const P2_GR = 0x00;

/* ISO 7816-4: 5.1.5 File Control Information, Table 1. For FCP and FMD. */
const TAG_PROPRIETARY = 0x00;
const TAG_NON_TLV = 0x53;
const TAG_BER_TLV = 0x73;

/* ASN.1 tags */
const TAG_SEQUENCE = 0x30;
const TAG_OCTETSTRING = 0x04;
const TAG_OID = 0x06; // Object Identifier

/* ISO 7816-4: 5.1.5 File Control Information, Templates. */
const TAG_FCP = 0x62; // File control parameters template
const TAG_FMD = 0x64; // File management data template
const TAG_FCI = 0x6F; // File control information template

/* EF_DIR tags */
const TAG_APPLTEMPLATE = 0x61;
const TAG_APPLIDENTIFIER = 0x4F;
const TAG_APPLLABEL = 0x50;
const TAG_APPLPATH = 0x51;

const TAG_GPD_ALL = 0x82; // EF-ACRules - GPD spec. "all applets"

/* Generic TLVs that are parsed */
const TAG_GPD_AID = 0xA0; // AID in the EF-ACRules - GPD spec, "one applet"
const TAG_EXTERNALDO = 0xA1; // External data objects - PKCS#15
const TAG_INDIRECT = 0xA5; // Indirect value.
const TAG_EF_ODF = 0xA7; // Elemenetary File Object Directory File

// This set should be what the actual ACE uses.
let containerTags = [
      TAG_SEQUENCE,
      TAG_FCP,
      TAG_GPD_AID,
      TAG_EXTERNALDO,
      TAG_INDIRECT,
      TAG_EF_ODF
];
