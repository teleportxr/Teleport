#pragma once

//! Signed Universal Manifest fixtures shared by the C++ manifest tests.
//!
//! GENERATED FILE — do not edit by hand. Regenerate with:
//!
//!   cd teleport-nodejs && node tools/make-manifest-fixtures.js \
//!       > ../Teleport/test/manifest_fixture.h
//!
//! Every fixture is signed by the Node.js implementation using the Ed25519 key
//! whose seed is the bytes 1..32. Verifying them here proves that two
//! independent canonicalisers and two independent Ed25519 stacks agree
//! byte-for-byte — which is the only thing standing between a protocol change
//! and a fleet of clients whose manifests silently stop verifying.
//!
//! A regenerated fixture whose signature value changes without an intended
//! change to its content means the canonicalisers have drifted. Investigate
//! rather than pasting in the new value.

namespace teleport
{
	namespace test
	{
		//! The baseline: one avatar pointer, one requested facet, one consent for it.
		//! Carries `somethingFromAFutureVersion`, a member no v0.3 consumer understands,
		//! holding nested objects with unsorted keys and non-ASCII text. Unknown members
		//! are covered by the signature, so this doubles as proof that forward
		//! compatibility does not cost verifiability.
		constexpr const char *kSignedManifestJson = R"JSON({
 "@context": "https://universalmanifest.net/ns/v0.3",
 "@id": "urn:uuid:6dfc40f2-8797-4f7b-a5f7-49d6a010f600",
 "@type": [
  "um:Manifest"
 ],
 "manifestVersion": "0.3",
 "subject": "did:web:xr.example:users:beta",
 "issuedAt": "2026-03-05T12:05:00Z",
 "expiresAt": "2036-03-05T12:05:00Z",
 "pointers": [
  {
   "@type": "um:Pointer",
   "name": "portableIdentity.avatar",
   "target": "https://assets.example/avatars/beta.glb"
  }
 ],
 "facets": [
  {
   "@id": "urn:facet:avatarProfile",
   "@type": [
    "um:Facet"
   ],
   "name": "avatarProfile",
   "entity": {
    "@id": "did:web:xr.example:users:beta#avatar",
    "@type": [
     "um:Entity",
     "xr:AvatarProfile"
    ],
    "skeletonProfile": "humanoid-v1",
    "supportsFacialBlendShapes": true
   }
  }
 ],
 "consents": [
  {
   "@id": "urn:consent:avatarProfile",
   "@type": "um:Consent",
   "facetRef": "urn:facet:avatarProfile",
   "scope": [
    "read",
    "display"
   ],
   "purpose": "avatar-presentation",
   "grantedAt": "2026-03-05T12:05:00Z",
   "expiresAt": "2036-03-05T12:05:00Z"
  }
 ],
 "somethingFromAFutureVersion": {
  "nested": [
   1,
   2,
   {
    "z": 1,
    "a": 2
   }
  ],
  "unicode": "héllo €"
 },
 "signature": {
  "algorithm": "Ed25519",
  "canonicalization": "JCS-RFC8785",
  "keyRef": "did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7",
  "value": "KOCqdwSRp2utPrJLf8ewIhV1v85ONlrlS0z0CkqoLEXgZ7osv_YB0_VUotraU1ciQnW26RzZPnYdXguERoz6AA"
 }
})JSON";

		//! The avatar pointer resolves, but the requested facet carries no consent.
		//! Default-deny means the facet is withheld while the avatar still resolves —
		//! partial acceptance is the normal outcome, not an error.
		constexpr const char *kManifestWithoutConsentJson = R"JSON({
 "@context": "https://universalmanifest.net/ns/v0.3",
 "@id": "urn:uuid:6dfc40f2-8797-4f7b-a5f7-49d6a010f600",
 "@type": [
  "um:Manifest"
 ],
 "manifestVersion": "0.3",
 "subject": "did:web:xr.example:users:beta",
 "issuedAt": "2026-03-05T12:05:00Z",
 "expiresAt": "2036-03-05T12:05:00Z",
 "pointers": [
  {
   "@type": "um:Pointer",
   "name": "portableIdentity.avatar",
   "target": "https://assets.example/avatars/beta.glb"
  }
 ],
 "facets": [
  {
   "@id": "urn:facet:avatarProfile",
   "@type": [
    "um:Facet"
   ],
   "name": "avatarProfile",
   "entity": {
    "@id": "did:web:xr.example:users:beta#avatar",
    "@type": [
     "um:Entity",
     "xr:AvatarProfile"
    ],
    "skeletonProfile": "humanoid-v1",
    "supportsFacialBlendShapes": true
   }
  }
 ],
 "consents": [],
 "signature": {
  "algorithm": "Ed25519",
  "canonicalization": "JCS-RFC8785",
  "keyRef": "did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7",
  "value": "L4BTxW9FupQ7BnQ6-EnDSQh2mxqdjINbjApn8HjKbYxVgka1ElFRETrAmBJxDFerESi4QlASz-zpV2_klTcJAQ"
 }
})JSON";

		//! A manifest whose only pointer is something other than an avatar.
		constexpr const char *kManifestWithoutAvatarPointerJson = R"JSON({
 "@context": "https://universalmanifest.net/ns/v0.3",
 "@id": "urn:uuid:6dfc40f2-8797-4f7b-a5f7-49d6a010f600",
 "@type": [
  "um:Manifest"
 ],
 "manifestVersion": "0.3",
 "subject": "did:web:xr.example:users:beta",
 "issuedAt": "2026-03-05T12:05:00Z",
 "expiresAt": "2036-03-05T12:05:00Z",
 "pointers": [
  {
   "@type": "um:Pointer",
   "name": "portableIdentity.wearables",
   "target": "https://assets.example/w.json"
  }
 ],
 "facets": [
  {
   "@id": "urn:facet:avatarProfile",
   "@type": [
    "um:Facet"
   ],
   "name": "avatarProfile",
   "entity": {
    "@id": "did:web:xr.example:users:beta#avatar",
    "@type": [
     "um:Entity",
     "xr:AvatarProfile"
    ],
    "skeletonProfile": "humanoid-v1",
    "supportsFacialBlendShapes": true
   }
  }
 ],
 "consents": [
  {
   "@id": "urn:consent:avatarProfile",
   "@type": "um:Consent",
   "facetRef": "urn:facet:avatarProfile",
   "scope": [
    "read",
    "display"
   ],
   "purpose": "avatar-presentation",
   "grantedAt": "2026-03-05T12:05:00Z",
   "expiresAt": "2036-03-05T12:05:00Z"
  }
 ],
 "signature": {
  "algorithm": "Ed25519",
  "canonicalization": "JCS-RFC8785",
  "keyRef": "did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7",
  "value": "HK9w37uAnLV8GZ0ccFHur-KfNTWeN2kg40UXpXu4xhD9mUjBOjq-hqVGRW-NPiAXm3qBRASgvdOnszsh1BjPCw"
 }
})JSON";

		//! Declares requiredTrustTier 1, which needs verifiable presentations or
		//! cross-DID binding. Tier 0 — signature-only — is all this implementation can
		//! verify, so it must refuse rather than silently downgrade.
		constexpr const char *kManifestRequiringTier1Json = R"JSON({
 "@context": "https://universalmanifest.net/ns/v0.3",
 "@id": "urn:uuid:6dfc40f2-8797-4f7b-a5f7-49d6a010f600",
 "@type": [
  "um:Manifest"
 ],
 "manifestVersion": "0.3",
 "subject": "did:web:xr.example:users:beta",
 "issuedAt": "2026-03-05T12:05:00Z",
 "expiresAt": "2036-03-05T12:05:00Z",
 "pointers": [
  {
   "@type": "um:Pointer",
   "name": "portableIdentity.avatar",
   "target": "https://assets.example/avatars/beta.glb"
  }
 ],
 "facets": [
  {
   "@id": "urn:facet:avatarProfile",
   "@type": [
    "um:Facet"
   ],
   "name": "avatarProfile",
   "entity": {
    "@id": "did:web:xr.example:users:beta#avatar",
    "@type": [
     "um:Entity",
     "xr:AvatarProfile"
    ],
    "skeletonProfile": "humanoid-v1",
    "supportsFacialBlendShapes": true
   }
  }
 ],
 "consents": [
  {
   "@id": "urn:consent:avatarProfile",
   "@type": "um:Consent",
   "facetRef": "urn:facet:avatarProfile",
   "scope": [
    "read",
    "display"
   ],
   "purpose": "avatar-presentation",
   "grantedAt": "2026-03-05T12:05:00Z",
   "expiresAt": "2036-03-05T12:05:00Z"
  }
 ],
 "requiredTrustTier": 1,
 "signature": {
  "algorithm": "Ed25519",
  "canonicalization": "JCS-RFC8785",
  "keyRef": "did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7",
  "value": "SUCucwRlP8Lm92arAw59ww4RRRgLVbq3y6ri-QA7xJ9ne1l53mjnJh_-ZyAuZuTdhtcX_IhkwvBNfbES1bScBA"
 }
})JSON";

		//! The subject has explicitly denied use of their avatar pointer.
		constexpr const char *kManifestWithDeniedAvatarJson = R"JSON({
 "@context": "https://universalmanifest.net/ns/v0.3",
 "@id": "urn:uuid:6dfc40f2-8797-4f7b-a5f7-49d6a010f600",
 "@type": [
  "um:Manifest"
 ],
 "manifestVersion": "0.3",
 "subject": "did:web:xr.example:users:beta",
 "issuedAt": "2026-03-05T12:05:00Z",
 "expiresAt": "2036-03-05T12:05:00Z",
 "pointers": [
  {
   "@type": "um:Pointer",
   "name": "portableIdentity.avatar",
   "target": "https://assets.example/avatars/beta.glb"
  }
 ],
 "facets": [
  {
   "@id": "urn:facet:avatarProfile",
   "@type": [
    "um:Facet"
   ],
   "name": "avatarProfile",
   "entity": {
    "@id": "did:web:xr.example:users:beta#avatar",
    "@type": [
     "um:Entity",
     "xr:AvatarProfile"
    ],
    "skeletonProfile": "humanoid-v1",
    "supportsFacialBlendShapes": true
   }
  }
 ],
 "consents": [
  {
   "@id": "urn:consent:avatarProfile",
   "@type": "um:Consent",
   "facetRef": "urn:facet:avatarProfile",
   "scope": [
    "read",
    "display"
   ],
   "purpose": "avatar-presentation",
   "grantedAt": "2026-03-05T12:05:00Z",
   "expiresAt": "2036-03-05T12:05:00Z"
  },
  {
   "@type": "um:Consent",
   "name": "portableIdentity.avatar",
   "value": "denied"
  }
 ],
 "signature": {
  "algorithm": "Ed25519",
  "canonicalization": "JCS-RFC8785",
  "keyRef": "did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7",
  "value": "RjX0nkqFMG16kah0-0MJjGIj1CoSjN745B169NXmvwk9aDP6WMCPMH2Q2b6ia5myzHj1iGkNoZ-uh3ZbzmG6CA"
 }
})JSON";

		//! A pointer target relative to the manifest url, so a manifest may point at an
		//! asset hosted beside it.
		constexpr const char *kManifestWithRelativeTargetJson = R"JSON({
 "@context": "https://universalmanifest.net/ns/v0.3",
 "@id": "urn:uuid:6dfc40f2-8797-4f7b-a5f7-49d6a010f600",
 "@type": [
  "um:Manifest"
 ],
 "manifestVersion": "0.3",
 "subject": "did:web:xr.example:users:beta",
 "issuedAt": "2026-03-05T12:05:00Z",
 "expiresAt": "2036-03-05T12:05:00Z",
 "pointers": [
  {
   "@type": "um:Pointer",
   "name": "portableIdentity.avatar",
   "target": "avatars/me.glb"
  }
 ],
 "facets": [
  {
   "@id": "urn:facet:avatarProfile",
   "@type": [
    "um:Facet"
   ],
   "name": "avatarProfile",
   "entity": {
    "@id": "did:web:xr.example:users:beta#avatar",
    "@type": [
     "um:Entity",
     "xr:AvatarProfile"
    ],
    "skeletonProfile": "humanoid-v1",
    "supportsFacialBlendShapes": true
   }
  }
 ],
 "consents": [
  {
   "@id": "urn:consent:avatarProfile",
   "@type": "um:Consent",
   "facetRef": "urn:facet:avatarProfile",
   "scope": [
    "read",
    "display"
   ],
   "purpose": "avatar-presentation",
   "grantedAt": "2026-03-05T12:05:00Z",
   "expiresAt": "2036-03-05T12:05:00Z"
  }
 ],
 "signature": {
  "algorithm": "Ed25519",
  "canonicalization": "JCS-RFC8785",
  "keyRef": "did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7",
  "value": "M5kZehpQxTbqf0avjlX0NBqdeO0D4qfbbixlQgkNE2Qndo6Y8Rixbs-gIEYnX-Ku5NjRbyMgAgtev3dBRLfDBA"
 }
})JSON";

		//! The requested facet is encrypted and we have no key. It is acknowledged as
		//! opaque and skipped — never grounds to reject the manifest.
		constexpr const char *kManifestWithSealedFacetJson = R"JSON({
 "@context": "https://universalmanifest.net/ns/v0.3",
 "@id": "urn:uuid:6dfc40f2-8797-4f7b-a5f7-49d6a010f600",
 "@type": [
  "um:Manifest"
 ],
 "manifestVersion": "0.3",
 "subject": "did:web:xr.example:users:beta",
 "issuedAt": "2026-03-05T12:05:00Z",
 "expiresAt": "2036-03-05T12:05:00Z",
 "pointers": [
  {
   "@type": "um:Pointer",
   "name": "portableIdentity.avatar",
   "target": "https://assets.example/avatars/beta.glb"
  }
 ],
 "facets": [
  {
   "@id": "urn:facet:avatarProfile",
   "@type": [
    "um:Facet"
   ],
   "name": "avatarProfile",
   "encryptionProfile": "jwe-inline-v1",
   "entity": {
    "protected": "eyJhbGciOiJFQ0RILUVTIn0",
    "ciphertext": "q1w2e3",
    "iv": "aXY",
    "tag": "dGFn"
   }
  }
 ],
 "consents": [],
 "signature": {
  "algorithm": "Ed25519",
  "canonicalization": "JCS-RFC8785",
  "keyRef": "did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7",
  "value": "02i3aS1jhbC2I7EZcC0ixzpMFs6bSQE3KN-pfqV2uuAwa_Qa45dqR1lyetuCg3LilAZ1g3nbjAqcmaMlvJXbDA"
 }
})JSON";

		constexpr const char *kFixtureDidKey    = "did:key:z6MkneMkZqwqRiU5mJzSG3kDwzt9P8C59N4NGTfBLfSGE7c7";
		constexpr const char *kFixtureSpkiB64   = "MCowBQYDK2VwAyEAebVWLo/mVPlAeLES6KmLp5AfhTrmlb7X4OORC60ElmQ=";
		constexpr const char *kFixtureAvatarUrl = "https://assets.example/avatars/beta.glb";

		//! The UTF-8 byte length of the baseline fixture's canonical signing input.
		//! Note the unit: this repository's canonicalize() returns a JS string, so
		//! String.length would report fewer UTF-16 code units. What gets signed is
		//! always the UTF-8 encoding (RFC 8785 §3.3).
		constexpr size_t kFixtureSigningInputBytes = 956;

		//! Comfortably inside every fixture's validity window (2027-01-15).
		constexpr long long kFixtureNow = 1800000000LL;
	}
}
