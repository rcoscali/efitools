#ifndef _INC_EFIAUTHENTICATED_H
#define _INC_EFIAUTHENTICATED_H
//***********************************************************************
// Signature Database
//***********************************************************************
///
/// The format of a signature database. 
///
#pragma pack(1)

typedef struct {
  ///
  /// An identifier which identifies the agent which added the signature to the list.
  ///
  EFI_GUID          SignatureOwner;
  ///
  /// The format of the signature is defined by the SignatureType.
  ///
  UINT8             SignatureData[1];
} EFI_SIGNATURE_DATA;

typedef struct {
  ///
  /// Type of the signature. GUID signature types are defined in below.
  ///
  EFI_GUID            SignatureType;
  ///
  /// Total size of the signature list, including this header.
  ///
  UINT32              SignatureListSize;
  ///
  /// Size of the signature header which precedes the array of signatures.
  ///
  UINT32              SignatureHeaderSize;
  ///
  /// Size of each signature.
  ///
  UINT32              SignatureSize; 
  ///
  /// Header before the array of signatures. The format of this header is specified 
  /// by the SignatureType.
  /// UINT8           SignatureHeader[SignatureHeaderSize];
  ///
  /// An array of signatures. Each signature is SignatureSize bytes in length. 
  /// EFI_SIGNATURE_DATA Signatures[][SignatureSize];
  ///
} EFI_SIGNATURE_LIST;

#pragma pack()

//
// _WIN_CERTIFICATE.wCertificateType
// 
#define WIN_CERT_TYPE_PKCS_SIGNED_DATA 0x0002
#define WIN_CERT_TYPE_EFI_PKCS115      0x0EF0
#define WIN_CERT_TYPE_EFI_GUID         0x0EF1

#define EFI_CERT_X509_GUID \
  (EFI_GUID){								\
    0xa5c059a1, 0x94e4, 0x4aa7, {0x87, 0xb5, 0xab, 0x15, 0x5c, 0x2b, 0xf0, 0x72} \
  }

#define EFI_CERT_RSA2048_GUID \
  (EFI_GUID){								\
    0x3c5766e8, 0x269c, 0x4e34, {0xaa, 0x14, 0xed, 0x77, 0x6e, 0x85, 0xb3, 0xb6} \
  }


#define EFI_CERT_TYPE_PKCS7_GUID \
  (EFI_GUID){								\
    0x4aafd29d, 0x68df, 0x49ee, {0x8a, 0xa9, 0x34, 0x7d, 0x37, 0x56, 0x65, 0xa7} \
  }

///
/// The WIN_CERTIFICATE structure is part of the PE/COFF specification.
///
typedef struct {
  ///
  /// The length of the entire certificate,  
  /// including the length of the header, in bytes.                                
  ///
  UINT32  dwLength;
  ///
  /// The revision level of the WIN_CERTIFICATE 
  /// structure. The current revision level is 0x0200.                                   
  ///
  UINT16  wRevision;
  ///
  /// The certificate type. See WIN_CERT_TYPE_xxx for the UEFI      
  /// certificate types. The UEFI specification reserves the range of 
  /// certificate type values from 0x0EF0 to 0x0EFF.                          
  ///
  UINT16  wCertificateType;
  ///
  /// The following is the actual certificate. The format of   
  /// the certificate depends on wCertificateType.
  ///
  /// UINT8 bCertificate[ANYSIZE_ARRAY];
  ///
} WIN_CERTIFICATE;

///
/// WIN_CERTIFICATE_UEFI_GUID.CertType
/// 
#define EFI_CERT_TYPE_RSA2048_SHA256_GUID \
  {0xa7717414, 0xc616, 0x4977, {0x94, 0x20, 0x84, 0x47, 0x12, 0xa7, 0x35, 0xbf } }

///
/// WIN_CERTIFICATE_UEFI_GUID.CertData
/// 
typedef struct {
  EFI_GUID  HashType;
  UINT8     PublicKey[256];
  UINT8     Signature[256];
} EFI_CERT_BLOCK_RSA_2048_SHA256;


///
/// Certificate which encapsulates a GUID-specific digital signature
///
typedef struct {
  ///
  /// This is the standard WIN_CERTIFICATE header, where
  /// wCertificateType is set to WIN_CERT_TYPE_UEFI_GUID. 
  ///                         
  WIN_CERTIFICATE   Hdr;
  ///
  /// This is the unique id which determines the 
  /// format of the CertData. .
  ///
  EFI_GUID          CertType;
  /// 
  /// The following is the certificate data. The format of
  /// the data is determined by the CertType. 
  /// If CertType is EFI_CERT_TYPE_RSA2048_SHA256_GUID,
  /// the CertData will be EFI_CERT_BLOCK_RSA_2048_SHA256 structure.
  ///
  UINT8            CertData[1];
} WIN_CERTIFICATE_UEFI_GUID;


///   
/// Certificate which encapsulates the RSASSA_PKCS1-v1_5 digital signature.
///  
/// The WIN_CERTIFICATE_UEFI_PKCS1_15 structure is derived from
/// WIN_CERTIFICATE and encapsulate the information needed to  
/// implement the RSASSA-PKCS1-v1_5 digital signature algorithm as  
/// specified in RFC2437.  
///  
typedef struct {     
  ///
  /// This is the standard WIN_CERTIFICATE header, where 
  /// wCertificateType is set to WIN_CERT_TYPE_UEFI_PKCS1_15.                       
  ///
  WIN_CERTIFICATE Hdr;
  ///
  /// This is the hashing algorithm which was performed on the
  /// UEFI executable when creating the digital signature. 
  ///
  EFI_GUID        HashAlgorithm;
  ///
  /// The following is the actual digital signature. The   
  /// size of the signature is the same size as the key 
  /// (1024-bit key is 128 bytes) and can be determined by 
  /// subtracting the length of the other parts of this header
  /// from the total length of the certificate as found in 
  /// Hdr.dwLength.                               
  ///
  /// UINT8 Signature[];
  ///
} WIN_CERTIFICATE_EFI_PKCS1_15;

#define OFFSET_OF(TYPE, Field) ((UINTN) &(((TYPE *)0)->Field))

///
/// Attributes of Authenticated Variable
///
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS              0x00000010
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS   0x00000020
#define EFI_VARIABLE_APPEND_WRITE                            0x00000040

///   
/// AuthInfo is a WIN_CERTIFICATE using the wCertificateType
/// WIN_CERTIFICATE_UEFI_GUID and the CertType
/// EFI_CERT_TYPE_RSA2048_SHA256_GUID. If the attribute specifies
/// authenticated access, then the Data buffer should begin with an
/// authentication descriptor prior to the data payload and DataSize
/// should reflect the the data.and descriptor size. The caller
/// shall digest the Monotonic Count value and the associated data
/// for the variable update using the SHA-256 1-way hash algorithm.
/// The ensuing the 32-byte digest will be signed using the private
/// key associated w/ the public/private 2048-bit RSA key-pair. The
/// WIN_CERTIFICATE shall be used to describe the signature of the
/// Variable data *Data. In addition, the signature will also
/// include the MonotonicCount value to guard against replay attacks.
///  
typedef struct {
  ///
  /// Included in the signature of        
  /// AuthInfo.Used to ensure freshness/no
  /// replay. Incremented during each     
  /// "Write" access.   
  ///                  
  UINT64                      MonotonicCount;
  ///
  /// Provides the authorization for the variable 
  /// access. It is a signature across the        
  /// variable data and the  Monotonic Count      
  /// value. Caller uses Private key that is      
  /// associated with a public key that has been  
  /// provisioned via the key exchange.           
  ///
  WIN_CERTIFICATE_UEFI_GUID   AuthInfo;
} EFI_VARIABLE_AUTHENTICATION;

///
/// When the attribute EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS is 
/// set, then the Data buffer shall begin with an instance of a complete (and serialized)
/// EFI_VARIABLE_AUTHENTICATION_2 descriptor. The descriptor shall be followed by the new 
/// variable value and DataSize shall reflect the combined size of the descriptor and the new 
/// variable value. The authentication descriptor is not part of the variable data and is not 
/// returned by subsequent calls to GetVariable().
///
typedef struct {
  ///
  /// For the TimeStamp value, components Pad1, Nanosecond, TimeZone, Daylight and 
  /// Pad2 shall be set to 0. This means that the time shall always be expressed in GMT.
  ///
  EFI_TIME                    TimeStamp;
  /// 
  /// Only a CertType of  EFI_CERT_TYPE_PKCS7_GUID is accepted.
  ///
  WIN_CERTIFICATE_UEFI_GUID   AuthInfo;
 } EFI_VARIABLE_AUTHENTICATION_2;

#endif
