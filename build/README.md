# Build & Signing

## Development Certificate

Run `create-test-cert.ps1` in an elevated PowerShell to generate a self-signed certificate for local MSIX testing.

## Distribution

For Microsoft Store or sideloading distribution, replace the test certificate with a trusted code-signing certificate:

1. Obtain a code-signing certificate from a trusted CA
2. Update the `PackageCertificateKeyFile` in the `.vcxproj`
3. Update the `Publisher` in `Package.appxmanifest` to match the certificate subject

## Build

```
MSBuild.exe "ManifoldDesktop.vcxproj" -p:Configuration=Release -p:Platform=x64
```
