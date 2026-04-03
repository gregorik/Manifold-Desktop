# Create a self-signed certificate for MSIX package signing
# Run this script once per development machine (elevated PowerShell)

$certName = "CN=Manifold Desktop Dev"
$cert = New-SelfSignedCertificate `
    -Type Custom `
    -Subject $certName `
    -KeyUsage DigitalSignature `
    -FriendlyName "Manifold Desktop Development Certificate" `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")

$password = ConvertTo-SecureString -String "ManifoldDev2026!" -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath "$PSScriptRoot\ManifoldDev.pfx" -Password $password

Write-Host "Certificate created and exported to ManifoldDev.pfx"
Write-Host "Thumbprint: $($cert.Thumbprint)"
