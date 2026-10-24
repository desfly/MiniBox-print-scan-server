#ifndef MINIBOX_M1522_IDENTITY_H
#define MINIBOX_M1522_IDENTITY_H

/*
 * Stable identity for this MiniBox MFP appliance.
 *
 * Human-visible service names must not contain this suffix. The UUID is used
 * only for protocol identity/correlation across IPP, DNS-SD/eSCL and WSD.
 * Network addresses remain dynamic and are never part of the identity.
 */
#define MINIBOX_MFP_SERIAL "0cefafcfc53d"
#define MINIBOX_MFP_UUID "4d424f58-0000-4000-8000-0cefafcfc53d"
#define MINIBOX_MFP_URN_UUID "urn:uuid:" MINIBOX_MFP_UUID

#endif
