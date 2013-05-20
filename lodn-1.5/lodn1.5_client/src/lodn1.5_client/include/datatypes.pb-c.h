/* Generated by the protocol buffer compiler.  DO NOT EDIT! */

#ifndef PROTOBUF_C_datatypes_2eproto__INCLUDED
#define PROTOBUF_C_datatypes_2eproto__INCLUDED

#include <google/protobuf-c/protobuf-c.h>

PROTOBUF_C_BEGIN_DECLS


typedef struct _Lodn__Stat Lodn__Stat;
typedef struct _Lodn__Capability Lodn__Capability;
typedef struct _Lodn__Metadata Lodn__Metadata;
typedef struct _Lodn__Function Lodn__Function;
typedef struct _Lodn__Mapping Lodn__Mapping;
typedef struct _Lodn__Exnode Lodn__Exnode;


/* --- enums --- */

typedef enum _Lodn__Capability__WRM {
  LODN__CAPABILITY__WRM__WRITE = 0,
  LODN__CAPABILITY__WRM__READ = 1,
  LODN__CAPABILITY__WRM__MANAGE = 2
} Lodn__Capability__WRM;
typedef enum _Lodn__Metadata__Type {
  LODN__METADATA__TYPE__NONE = 0,
  LODN__METADATA__TYPE__STRING = 1,
  LODN__METADATA__TYPE__INTEGER = 2,
  LODN__METADATA__TYPE__DOUBLE = 3,
  LODN__METADATA__TYPE__METADATA = 4
} Lodn__Metadata__Type;

/* --- messages --- */

struct  _Lodn__Stat
{
  ProtobufCMessage base;
  protobuf_c_boolean has_inode;
  uint64_t inode;
  protobuf_c_boolean has_mode;
  uint32_t mode;
  protobuf_c_boolean has_nlinks;
  uint32_t nlinks;
  protobuf_c_boolean has_size;
  uint64_t size;
  protobuf_c_boolean has_blocks;
  uint32_t blocks;
  protobuf_c_boolean has_atime;
  uint32_t atime;
  protobuf_c_boolean has_mtime;
  uint32_t mtime;
  protobuf_c_boolean has_ctime;
  uint32_t ctime;
  protobuf_c_boolean has_btime;
  uint32_t btime;
  protobuf_c_boolean has_st_blksize;
  uint32_t st_blksize;
};
#define LODN__STAT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&lodn__stat__descriptor) \
    , 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,0 }


struct  _Lodn__Capability
{
  ProtobufCMessage base;
  char *host;
  protobuf_c_boolean has_port;
  uint32_t port;
  char *rid;
  char *key;
  protobuf_c_boolean has_wrm;
  Lodn__Capability__WRM wrm;
};
#define LODN__CAPABILITY__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&lodn__capability__descriptor) \
    , NULL, 0,0, NULL, NULL, 0,0 }


struct  _Lodn__Metadata
{
  ProtobufCMessage base;
  char *name;
  protobuf_c_boolean has_type;
  Lodn__Metadata__Type type;
  char *value;
};
#define LODN__METADATA__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&lodn__metadata__descriptor) \
    , NULL, 0,0, NULL }


struct  _Lodn__Function
{
  ProtobufCMessage base;
  char *name;
  size_t n_metadata;
  Lodn__Metadata **metadata;
  size_t n_arguments;
  Lodn__Metadata **arguments;
  Lodn__Function *subfunction;
};
#define LODN__FUNCTION__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&lodn__function__descriptor) \
    , NULL, 0,NULL, 0,NULL, NULL }


struct  _Lodn__Mapping
{
  ProtobufCMessage base;
  protobuf_c_boolean has_alloc_length;
  uint64_t alloc_length;
  protobuf_c_boolean has_alloc_offset;
  uint64_t alloc_offset;
  protobuf_c_boolean has_e2e_blocksize;
  uint64_t e2e_blocksize;
  protobuf_c_boolean has_exnode_offset;
  uint64_t exnode_offset;
  protobuf_c_boolean has_logical_length;
  uint64_t logical_length;
  char *host;
  protobuf_c_boolean has_port;
  uint32_t port;
  char *rid;
  char *readkey;
  char *writekey;
  char *managekey;
  size_t n_metadata;
  Lodn__Metadata **metadata;
  char *function;
};
#define LODN__MAPPING__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&lodn__mapping__descriptor) \
    , 0,0, 0,0, 0,0, 0,0, 0,0, NULL, 0,0, NULL, NULL, NULL, NULL, 0,NULL, NULL }


struct  _Lodn__Exnode
{
  ProtobufCMessage base;
  size_t n_metadata;
  Lodn__Metadata **metadata;
  size_t n_mappings;
  Lodn__Mapping **mappings;
};
#define LODN__EXNODE__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&lodn__exnode__descriptor) \
    , 0,NULL, 0,NULL }


/* Lodn__Stat methods */
void   lodn__stat__init
                     (Lodn__Stat         *message);
size_t lodn__stat__get_packed_size
                     (const Lodn__Stat   *message);
size_t lodn__stat__pack
                     (const Lodn__Stat   *message,
                      uint8_t             *out);
size_t lodn__stat__pack_to_buffer
                     (const Lodn__Stat   *message,
                      ProtobufCBuffer     *buffer);
Lodn__Stat *
       lodn__stat__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   lodn__stat__free_unpacked
                     (Lodn__Stat *message,
                      ProtobufCAllocator *allocator);
/* Lodn__Capability methods */
void   lodn__capability__init
                     (Lodn__Capability         *message);
size_t lodn__capability__get_packed_size
                     (const Lodn__Capability   *message);
size_t lodn__capability__pack
                     (const Lodn__Capability   *message,
                      uint8_t             *out);
size_t lodn__capability__pack_to_buffer
                     (const Lodn__Capability   *message,
                      ProtobufCBuffer     *buffer);
Lodn__Capability *
       lodn__capability__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   lodn__capability__free_unpacked
                     (Lodn__Capability *message,
                      ProtobufCAllocator *allocator);
/* Lodn__Metadata methods */
void   lodn__metadata__init
                     (Lodn__Metadata         *message);
size_t lodn__metadata__get_packed_size
                     (const Lodn__Metadata   *message);
size_t lodn__metadata__pack
                     (const Lodn__Metadata   *message,
                      uint8_t             *out);
size_t lodn__metadata__pack_to_buffer
                     (const Lodn__Metadata   *message,
                      ProtobufCBuffer     *buffer);
Lodn__Metadata *
       lodn__metadata__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   lodn__metadata__free_unpacked
                     (Lodn__Metadata *message,
                      ProtobufCAllocator *allocator);
/* Lodn__Function methods */
void   lodn__function__init
                     (Lodn__Function         *message);
size_t lodn__function__get_packed_size
                     (const Lodn__Function   *message);
size_t lodn__function__pack
                     (const Lodn__Function   *message,
                      uint8_t             *out);
size_t lodn__function__pack_to_buffer
                     (const Lodn__Function   *message,
                      ProtobufCBuffer     *buffer);
Lodn__Function *
       lodn__function__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   lodn__function__free_unpacked
                     (Lodn__Function *message,
                      ProtobufCAllocator *allocator);
/* Lodn__Mapping methods */
void   lodn__mapping__init
                     (Lodn__Mapping         *message);
size_t lodn__mapping__get_packed_size
                     (const Lodn__Mapping   *message);
size_t lodn__mapping__pack
                     (const Lodn__Mapping   *message,
                      uint8_t             *out);
size_t lodn__mapping__pack_to_buffer
                     (const Lodn__Mapping   *message,
                      ProtobufCBuffer     *buffer);
Lodn__Mapping *
       lodn__mapping__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   lodn__mapping__free_unpacked
                     (Lodn__Mapping *message,
                      ProtobufCAllocator *allocator);
/* Lodn__Exnode methods */
void   lodn__exnode__init
                     (Lodn__Exnode         *message);
size_t lodn__exnode__get_packed_size
                     (const Lodn__Exnode   *message);
size_t lodn__exnode__pack
                     (const Lodn__Exnode   *message,
                      uint8_t             *out);
size_t lodn__exnode__pack_to_buffer
                     (const Lodn__Exnode   *message,
                      ProtobufCBuffer     *buffer);
Lodn__Exnode *
       lodn__exnode__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   lodn__exnode__free_unpacked
                     (Lodn__Exnode *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*Lodn__Stat_Closure)
                 (const Lodn__Stat *message,
                  void *closure_data);
typedef void (*Lodn__Capability_Closure)
                 (const Lodn__Capability *message,
                  void *closure_data);
typedef void (*Lodn__Metadata_Closure)
                 (const Lodn__Metadata *message,
                  void *closure_data);
typedef void (*Lodn__Function_Closure)
                 (const Lodn__Function *message,
                  void *closure_data);
typedef void (*Lodn__Mapping_Closure)
                 (const Lodn__Mapping *message,
                  void *closure_data);
typedef void (*Lodn__Exnode_Closure)
                 (const Lodn__Exnode *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor lodn__stat__descriptor;
extern const ProtobufCMessageDescriptor lodn__capability__descriptor;
extern const ProtobufCEnumDescriptor    lodn__capability__wrm__descriptor;
extern const ProtobufCMessageDescriptor lodn__metadata__descriptor;
extern const ProtobufCEnumDescriptor    lodn__metadata__type__descriptor;
extern const ProtobufCMessageDescriptor lodn__function__descriptor;
extern const ProtobufCMessageDescriptor lodn__mapping__descriptor;
extern const ProtobufCMessageDescriptor lodn__exnode__descriptor;

PROTOBUF_C_END_DECLS


#endif  /* PROTOBUF_datatypes_2eproto__INCLUDED */