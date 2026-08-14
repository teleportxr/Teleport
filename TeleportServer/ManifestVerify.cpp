#include "TeleportServer/ManifestVerify.h"

#include <algorithm>
#include <cstring>

#include <openssl/evp.h>

#include "TeleportCore/Jcs.h"

using nlohmann::json;

namespace teleport
{
	namespace server
	{
		namespace
		{
			constexpr const char *kSupportedAlgorithm		= "Ed25519";
			constexpr const char *kSupportedCanonicalization = "JCS-RFC8785";
			constexpr size_t      kEd25519RawKeyBytes		= 32;

			const char *kBase58Alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

			//! Accept both base64 and base64url. The profile names base64, but
			//! the two differ in only two characters and refusing a url-safe
			//! encoding here would be gratuitous.
			int Base64Value(char c)
			{
				if (c >= 'A' && c <= 'Z') return c - 'A';
				if (c >= 'a' && c <= 'z') return c - 'a' + 26;
				if (c >= '0' && c <= '9') return c - '0' + 52;
				if (c == '+' || c == '-') return 62;
				if (c == '/' || c == '_') return 63;
				return -1;
			}

			std::string StringMember(const json &j, const char *key)
			{
				if (!j.is_object() || !j.contains(key) || !j.at(key).is_string())
					return {};
				return j.at(key).get<std::string>();
			}
		}

		bool Base58Decode(const std::string &text, std::vector<uint8_t> &out)
		{
			out.clear();
			if (text.empty())
				return false;
			std::vector<uint8_t> bytes{ 0 };
			for (const char ch : text)
			{
				const char *pos = std::strchr(kBase58Alphabet, ch);
				if (!pos || ch == '\0')
					return false;
				int carry = static_cast<int>(pos - kBase58Alphabet);
				for (size_t i = 0; i < bytes.size(); i++)
				{
					carry += bytes[i] * 58;
					bytes[i] = static_cast<uint8_t>(carry & 0xFF);
					carry >>= 8;
				}
				while (carry)
				{
					bytes.push_back(static_cast<uint8_t>(carry & 0xFF));
					carry >>= 8;
				}
			}
			// Every leading '1' is a leading zero byte the arithmetic above
			// cannot represent.
			for (size_t i = 0; i < text.size() && text[i] == '1'; i++)
				bytes.push_back(0);
			out.assign(bytes.rbegin(), bytes.rend());
			return true;
		}

		bool Base64Decode(const std::string &text, std::vector<uint8_t> &out)
		{
			out.clear();
			int accumulator = 0;
			int bits = 0;
			for (const char ch : text)
			{
				if (ch == '=' || ch == '\n' || ch == '\r')
					continue;
				const int value = Base64Value(ch);
				if (value < 0)
					return false;
				accumulator = (accumulator << 6) | value;
				bits += 6;
				if (bits >= 8)
				{
					bits -= 8;
					out.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xFF));
				}
			}
			return true;
		}

		bool KeyFromDidKey(const std::string &did, std::vector<uint8_t> &rawOut)
		{
			static const std::string prefix = "did:key:z";
			if (did.compare(0, prefix.size(), prefix) != 0)
				return false;
			// Strip any fragment: did:key:zAbc#zAbc names the same key.
			std::string encoded = did.substr(prefix.size());
			const size_t hash = encoded.find('#');
			if (hash != std::string::npos)
				encoded = encoded.substr(0, hash);

			std::vector<uint8_t> decoded;
			if (!Base58Decode(encoded, decoded))
				return false;
			// 0xed 0x01 is the Ed25519 multicodec prefix. Anything else is a
			// different key type and cannot verify a signature.
			if (decoded.size() != kEd25519RawKeyBytes + 2 || decoded[0] != 0xed || decoded[1] != 0x01)
				return false;
			rawOut.assign(decoded.begin() + 2, decoded.end());
			return true;
		}

		bool KeyFromSpkiBase64(const std::string &b64, std::vector<uint8_t> &rawOut)
		{
			if (b64.empty())
				return false;
			std::vector<uint8_t> der;
			if (!Base64Decode(b64, der) || der.size() < kEd25519RawKeyBytes)
				return false;
			// An Ed25519 SubjectPublicKeyInfo is a fixed 12-byte header
			// followed by the raw key.
			rawOut.assign(der.end() - static_cast<long>(kEd25519RawKeyBytes), der.end());
			return true;
		}

		bool KeyFromJwkDocument(const json &doc, const std::string &kid, std::vector<uint8_t> &rawOut)
		{
			if (!doc.is_object())
				return false;
			std::vector<const json *> candidates;
			if (doc.contains("keys") && doc.at("keys").is_array())
			{
				for (const auto &k : doc.at("keys"))
					candidates.push_back(&k);
			}
			else
			{
				candidates.push_back(&doc);
			}

			for (const json *k : candidates)
			{
				if (!k->is_object())
					continue;
				if (StringMember(*k, "kty") != "OKP" || StringMember(*k, "crv") != "Ed25519")
					continue;
				const std::string x = StringMember(*k, "x");
				if (x.empty())
					continue;
				if (!kid.empty() && StringMember(*k, "kid") != kid)
					continue;
				std::vector<uint8_t> raw;
				if (Base64Decode(x, raw) && raw.size() == kEd25519RawKeyBytes)
				{
					rawOut = raw;
					return true;
				}
			}
			return false;
		}

		bool Ed25519Verify(const std::vector<uint8_t> &rawKey, const std::string &message,
			const std::vector<uint8_t> &signature)
		{
			if (rawKey.size() != kEd25519RawKeyBytes || signature.empty())
				return false;

			EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, rawKey.data(), rawKey.size());
			if (!pkey)
				return false;

			bool ok = false;
			EVP_MD_CTX *ctx = EVP_MD_CTX_new();
			if (ctx)
			{
				// Ed25519 is a one-shot scheme: no separate digest, and
				// EVP_DigestVerify rather than EVP_DigestVerifyFinal.
				if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1)
				{
					ok = EVP_DigestVerify(ctx, signature.data(), signature.size(),
						reinterpret_cast<const uint8_t *>(message.data()), message.size()) == 1;
				}
				EVP_MD_CTX_free(ctx);
			}
			EVP_PKEY_free(pkey);
			return ok;
		}

		std::string ManifestSigningInput(const json &manifest)
		{
			// Removing `signature` is what stops the signature covering
			// itself. Everything else stays, including members this
			// implementation does not understand — they are covered too,
			// which is what makes forward compatibility safe rather than a
			// hole.
			json payload = manifest;
			payload.erase("signature");
			return core::CanonicalizeJson(payload);
		}

		std::string CheckManifestFreshness(const json &manifest, int64_t nowUnix, int64_t clockSkewSeconds)
		{
			int64_t issued = 0;
			int64_t expires = 0;
			const std::string issuedAt  = StringMember(manifest, "issuedAt");
			const std::string expiresAt = StringMember(manifest, "expiresAt");
			if (!core::ParseRfc3339(issuedAt, issued) || !core::ParseRfc3339(expiresAt, expires))
				return "expired";
			if (nowUnix > expires + clockSkewSeconds)
				return "expired";
			if (issued > nowUnix + clockSkewSeconds)
				return "stale";
			return "fresh";
		}

		namespace
		{
			//! Resolve signature.keyRef to a raw public key, enforcing the
			//! keyRef/inline-key consistency check.
			bool ResolveKey(const core::ManifestSignature &signature, ManifestKeyFetchFn keyFetch,
				std::vector<uint8_t> &rawOut, std::string &reasonOut)
			{
				if (signature.keyRef.empty())
				{
					reasonOut = "manifest_key_unresolvable";
					return false;
				}

				std::vector<uint8_t> resolved;
				bool haveResolved = false;

				if (signature.keyRef.rfind("did:key:", 0) == 0)
				{
					haveResolved = KeyFromDidKey(signature.keyRef, resolved);
				}
				else if (signature.keyRef.rfind("https:", 0) == 0)
				{
					if (!keyFetch)
					{
						reasonOut = "manifest_key_unresolvable";
						return false;
					}
					const size_t hash = signature.keyRef.find('#');
					const std::string url = hash == std::string::npos ? signature.keyRef : signature.keyRef.substr(0, hash);
					const std::string kid = hash == std::string::npos ? std::string() : signature.keyRef.substr(hash + 1);
					std::string body;
					if (keyFetch(url, body))
					{
						const json doc = json::parse(body, nullptr, false);
						if (!doc.is_discarded())
							haveResolved = KeyFromJwkDocument(doc, kid, resolved);
					}
				}
				else
				{
					// did:web and anything else: not supported yet. Refusing
					// is the conformant outcome — skipping key resolution
					// because an inline key is present is precisely the
					// non-conformance that opens key substitution.
					reasonOut = "manifest_key_unresolvable";
					return false;
				}

				if (!haveResolved)
				{
					reasonOut = "manifest_key_unresolvable";
					return false;
				}

				// Consistency check: both routes must arrive at the same key.
				if (signature.publicKeySpkiB64.has_value())
				{
					std::vector<uint8_t> inlineKey;
					if (!KeyFromSpkiBase64(*signature.publicKeySpkiB64, inlineKey) || inlineKey != resolved)
					{
						reasonOut = "manifest_signature_invalid";
						return false;
					}
				}

				rawOut = resolved;
				return true;
			}
		}

		ManifestVerifyResult VerifyManifest(const json &manifest, int64_t nowUnix,
			int64_t clockSkewSeconds, ManifestKeyFetchFn keyFetch)
		{
			ManifestVerifyResult result;
			result.freshnessCheck = CheckManifestFreshness(manifest, nowUnix, clockSkewSeconds);
			if (result.freshnessCheck != "fresh")
				result.reasons.push_back("manifest_expired");

			if (!manifest.is_object() || !manifest.contains("signature") || !manifest.at("signature").is_object())
			{
				result.signatureCheck = "unsupported-profile";
				result.reasons.push_back("manifest_signature_invalid");
				return result;
			}

			core::ManifestSignature signature = manifest.at("signature").get<core::ManifestSignature>();
			if (signature.algorithm != kSupportedAlgorithm || signature.canonicalization != kSupportedCanonicalization)
			{
				result.signatureCheck = "unsupported-profile";
				result.reasons.push_back("manifest_signature_invalid");
				return result;
			}

			std::vector<uint8_t> rawKey;
			std::string reason;
			if (!ResolveKey(signature, keyFetch, rawKey, reason))
			{
				result.signatureCheck = "invalid";
				result.reasons.push_back(reason);
				return result;
			}

			std::vector<uint8_t> sig;
			if (!Base64Decode(signature.value, sig) ||
				!Ed25519Verify(rawKey, ManifestSigningInput(manifest), sig))
			{
				result.signatureCheck = "invalid";
				result.reasons.push_back("manifest_signature_invalid");
				return result;
			}

			result.signatureCheck = "valid";
			return result;
		}
	}
}
