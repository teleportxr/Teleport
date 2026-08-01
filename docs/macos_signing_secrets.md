# macOS signing and notarisation secrets

`.github/workflows/build_macos_client.yml` signs `teleport_terminal`, signs the `.pkg`, and
submits it to Apple for notarisation. This page describes how to create each of the nine
repository secrets it reads.

If any one of them is missing, the workflow's `check-secrets` job says so and the macOS build
job is **skipped**, not failed. A skipped job is neutral in GitHub Actions, so the run
finishes green: a fork, or a pull request from a fork, is never blocked by credentials it had
no way of seeing. The names of the missing secrets appear in the run summary.

The consequence is that no `macos-installer` artifact is produced. `release.yml` requires that
artifact for the tagged commit, so **without these secrets, tagging will not produce a
release** — it fails at the download step with "no artifact found" and publishes nothing. That
is deliberate: an unsigned macOS package is not something to put on a release page.

**Prerequisite:** an active [Apple Developer Program](https://developer.apple.com/programs/)
membership (£79/$99 per year). A free Apple ID cannot issue Developer ID certificates, which
are the only kind Gatekeeper accepts for software distributed outside the App Store.

Every step below needs a Mac — `Keychain Access` and `security` are macOS-only.

---

## Summary

| Secret | What it is |
|---|---|
| `MACOS_CERT_P12` | base64 of the *Developer ID Application* certificate + private key |
| `MACOS_CERT_PASSWORD` | the password you chose when exporting that `.p12` |
| `MACOS_INSTALLER_CERT_P12` | base64 of the *Developer ID Installer* certificate + private key |
| `MACOS_INSTALLER_CERT_PASSWORD` | the password you chose when exporting that `.p12` |
| `APPLE_DEVELOPER_NAME` | the name inside the certificates, e.g. `Simul Software Ltd` |
| `APPLE_TEAM_ID` | your 10-character Team ID, e.g. `A1B2C3D4E5` |
| `APPLE_API_KEY_P8` | base64 of the App Store Connect API key `.p8` |
| `APPLE_API_KEY_ID` | that key's 10-character Key ID |
| `APPLE_API_ISSUER_ID` | the issuer UUID for your App Store Connect account |

Add each at **Settings → Secrets and variables → Actions → New repository secret** on
`github.com/simul/Teleport`.

---

## 1. Find your Team ID → `APPLE_TEAM_ID`

Sign in at [developer.apple.com/account](https://developer.apple.com/account) and look under
**Membership details**. The **Team ID** is a 10-character alphanumeric string.

It also appears in parentheses at the end of every certificate name, so you can read it back
off step 2's output.

---

## 2. Create the two Developer ID certificates

Two *different* certificate types are needed, and they are not interchangeable:

- **Developer ID Application** — signs the `teleport_terminal` executable.
- **Developer ID Installer** — signs the `.pkg`.

For each one:

1. Open **Keychain Access** → menu **Keychain Access → Certificate Assistant → Request a
   Certificate From a Certificate Authority**.
2. Enter your email and a common name; select **Saved to disk**; leave CA Email blank.
   This produces a `.certSigningRequest` file.
3. Go to [developer.apple.com/account/resources/certificates/list](https://developer.apple.com/account/resources/certificates/list)
   → **+** → choose **Developer ID Application** (then repeat for **Developer ID Installer**).
4. Upload the `.certSigningRequest`, then download the resulting `.cer`.
5. Double-click the `.cer` to install it into your login keychain.

> Developer ID certificates are limited in number per account and **cannot be re-downloaded
> with their private key**. Keep the exported `.p12` files somewhere safe — losing them means
> revoking and reissuing.

Verify both are present and note the exact names:

```bash
security find-identity -v -p codesigning
```

You should see lines like:

```
1) ABC123... "Developer ID Application: Simul Software Ltd (A1B2C3D4E5)"
2) DEF456... "Developer ID Installer: Simul Software Ltd (A1B2C3D4E5)"
```

---

## 3. `APPLE_DEVELOPER_NAME`

The part between `Developer ID Application: ` and ` (TEAMID)` above — in the example,
`Simul Software Ltd`. The workflow reassembles the full identity string from this plus
`APPLE_TEAM_ID`, so it must match character for character, including any `Ltd`/`Inc` suffix.

---

## 4. Export the certificates → `MACOS_CERT_P12`, `MACOS_INSTALLER_CERT_P12`

In **Keychain Access**, under **login → My Certificates**, find each certificate, expand it to
confirm a private key is attached, then right-click → **Export**. Choose
**Personal Information Exchange (.p12)** and set a strong password.

Those passwords become `MACOS_CERT_PASSWORD` and `MACOS_INSTALLER_CERT_PASSWORD`.

GitHub secrets hold text, not binaries, so base64-encode each file:

```bash
base64 -i DeveloperIDApplication.p12 | pbcopy   # paste into MACOS_CERT_P12
base64 -i DeveloperIDInstaller.p12   | pbcopy   # paste into MACOS_INSTALLER_CERT_P12
```

`base64` on macOS emits a single line by default, which is what you want. On Linux use
`base64 -w0`.

---

## 5. Create an App Store Connect API key → `APPLE_API_KEY_P8`, `APPLE_API_KEY_ID`, `APPLE_API_ISSUER_ID`

This is what `notarytool` authenticates with. An API key is preferable to an Apple ID plus
app-specific password: it is not tied to one person's account, it can be revoked on its own,
and it is unaffected by that person enabling or changing two-factor authentication.

1. Go to [appstoreconnect.apple.com/access/integrations/api](https://appstoreconnect.apple.com/access/integrations/api).
2. Select the **Team Keys** tab.
3. **+** → name it something like `Teleport CI notarisation` → role **Developer**.
   (Developer is sufficient for notarisation; do not grant Admin.)
4. Download the `.p8` file. **It can only be downloaded once.**
5. The table shows the **KEY ID** (10 characters) → `APPLE_API_KEY_ID`, and the
   **Issuer ID** (a UUID, shown above the table) → `APPLE_API_ISSUER_ID`.

Encode the key the same way:

```bash
base64 -i AuthKey_XXXXXXXXXX.p8 | pbcopy    # paste into APPLE_API_KEY_P8
```

---

## 6. Verify locally before trusting CI

Worth doing once end to end on your own Mac, because a signing problem is much easier to
diagnose there than in a CI log:

```bash
cmake -S . -B build_macos -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DTELEPORT_GUI_CLIENT=OFF -DTELEPORT_CLIENT_USE_VULKAN=OFF \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" \
  -DTELEPORT_MACOS_INSTALLER_IDENTITY="Developer ID Installer: Simul Software Ltd (A1B2C3D4E5)"
cmake --build build_macos

codesign --force --options runtime --timestamp \
  --sign "Developer ID Application: Simul Software Ltd (A1B2C3D4E5)" \
  build_macos/bin/teleport_terminal

cd build_macos && TELEPORT_COMMIT=$(git rev-parse --short HEAD) cpack

xcrun notarytool submit teleportxr-*.pkg \
  --key AuthKey_XXXXXXXXXX.p8 --key-id XXXXXXXXXX --issuer <issuer-uuid> --wait
xcrun stapler staple teleportxr-*.pkg
spctl --assess --type install -vv teleportxr-*.pkg      # expect: source=Notarized Developer ID
```

The strongest check is on a *different* Mac from the one that built it: the build machine's
keychain can mask a Gatekeeper failure that a user would hit.

---

## Notes and failure modes

- **`--options runtime` is not optional.** Notarisation rejects binaries without the hardened
  runtime, and the error message does not say so clearly.
- **`--timestamp` is not optional either.** A signature without a secure timestamp stops
  validating as soon as the certificate expires.
- **If notarisation is rejected**, get the detail with
  `xcrun notarytool log <submission-id> --key ... --key-id ... --issuer ...`. The `--wait`
  output alone only says "Invalid".
- **Certificates expire after five years**; the API key does not expire but can be revoked.
  Diarise the certificate expiry — an expired Developer ID silently starts failing releases.
- **Stapling matters.** Without `stapler staple`, Gatekeeper has to reach Apple over the
  network to check the ticket, so a user who is offline or behind a restrictive firewall sees
  the package refused.
- **The workflow deletes its keychain** in an `if: always()` step, so the imported private keys
  do not outlive the job.
