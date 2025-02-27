// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/token_binding.h"

#include <openssl/bytestring.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/mem.h>

#include "base/stl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_config.h"

namespace net {

namespace {

enum TokenBindingType {
  TB_TYPE_PROVIDED = 0,
  TB_TYPE_REFERRED = 1,
};

bool BuildTokenBindingID(crypto::ECPrivateKey* key, CBB* out) {
  EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(key->key());
  DCHECK(ec_key);

  CBB ec_point;
  return CBB_add_u8(out, TB_PARAM_ECDSAP256) &&
         CBB_add_u8_length_prefixed(out, &ec_point) &&
         EC_POINT_point2cbb(&ec_point, EC_KEY_get0_group(ec_key),
                            EC_KEY_get0_public_key(ec_key),
                            POINT_CONVERSION_UNCOMPRESSED, nullptr) &&
         CBB_flush(out);
}

Error BuildTokenBinding(TokenBindingType type,
                        crypto::ECPrivateKey* key,
                        const std::vector<uint8_t>& signed_ekm,
                        std::string* out) {
  uint8_t* out_data;
  size_t out_len;
  CBB token_binding;
  if (!CBB_init(&token_binding, 0) || !CBB_add_u8(&token_binding, type) ||
      !BuildTokenBindingID(key, &token_binding) ||
      !CBB_add_u16(&token_binding, signed_ekm.size()) ||
      !CBB_add_bytes(&token_binding, signed_ekm.data(), signed_ekm.size()) ||
      // 0-length extensions
      !CBB_add_u16(&token_binding, 0) ||
      !CBB_finish(&token_binding, &out_data, &out_len)) {
    CBB_cleanup(&token_binding);
    return ERR_FAILED;
  }
  out->assign(reinterpret_cast<char*>(out_data), out_len);
  OPENSSL_free(out_data);
  return OK;
}

}  // namespace

Error BuildTokenBindingMessageFromTokenBindings(
    const std::vector<base::StringPiece>& token_bindings,
    std::string* out) {
  CBB tb_message, child;
  if (!CBB_init(&tb_message, 0) ||
      !CBB_add_u16_length_prefixed(&tb_message, &child)) {
    CBB_cleanup(&tb_message);
    return ERR_FAILED;
  }
  for (const base::StringPiece& token_binding : token_bindings) {
    if (!CBB_add_bytes(&child,
                       reinterpret_cast<const uint8_t*>(token_binding.data()),
                       token_binding.size())) {
      CBB_cleanup(&tb_message);
      return ERR_FAILED;
    }
  }

  uint8_t* out_data;
  size_t out_len;
  if (!CBB_finish(&tb_message, &out_data, &out_len)) {
    CBB_cleanup(&tb_message);
    return ERR_FAILED;
  }
  out->assign(reinterpret_cast<char*>(out_data), out_len);
  OPENSSL_free(out_data);
  return OK;
}

Error BuildProvidedTokenBinding(crypto::ECPrivateKey* key,
                                const std::vector<uint8_t>& signed_ekm,
                                std::string* out) {
  return BuildTokenBinding(TB_TYPE_PROVIDED, key, signed_ekm, out);
}

bool ParseTokenBindingMessage(base::StringPiece token_binding_message,
                              base::StringPiece* ec_point_out,
                              base::StringPiece* signature_out) {
  CBS tb_message, tb, ec_point, signature;
  uint8_t tb_type, tb_param;
  CBS_init(&tb_message,
           reinterpret_cast<const uint8_t*>(token_binding_message.data()),
           token_binding_message.size());
  if (!CBS_get_u16_length_prefixed(&tb_message, &tb) ||
      !CBS_get_u8(&tb, &tb_type) || !CBS_get_u8(&tb, &tb_param) ||
      !CBS_get_u8_length_prefixed(&tb, &ec_point) ||
      !CBS_get_u16_length_prefixed(&tb, &signature) ||
      tb_type != TB_TYPE_PROVIDED || tb_param != TB_PARAM_ECDSAP256) {
    return false;
  }

  *ec_point_out = base::StringPiece(
      reinterpret_cast<const char*>(CBS_data(&ec_point)), CBS_len(&ec_point));
  *signature_out = base::StringPiece(
      reinterpret_cast<const char*>(CBS_data(&signature)), CBS_len(&signature));
  return true;
}

bool VerifyEKMSignature(base::StringPiece ec_point,
                        base::StringPiece signature,
                        base::StringPiece ekm) {
  crypto::ScopedEC_Key key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  EC_KEY* keyp = key.get();
  const uint8_t* ec_point_data =
      reinterpret_cast<const uint8_t*>(ec_point.data());
  if (o2i_ECPublicKey(&keyp, &ec_point_data, ec_point.size()) != key.get())
    return false;
  crypto::ScopedEVP_PKEY pkey(EVP_PKEY_new());
  if (!EVP_PKEY_assign_EC_KEY(pkey.get(), key.release()))
    return false;
  crypto::ScopedEVP_PKEY_CTX pctx(EVP_PKEY_CTX_new(pkey.get(), nullptr));
  if (!EVP_PKEY_verify_init(pctx.get()) ||
      !EVP_PKEY_verify(
          pctx.get(), reinterpret_cast<const uint8_t*>(signature.data()),
          signature.size(), reinterpret_cast<const uint8_t*>(ekm.data()),
          ekm.size())) {
    return false;
  }
  return true;
}

}  // namespace net
