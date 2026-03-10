Add-Type -AssemblyName System.Drawing
$dir = "docs/Pics"
$outDir = "docs/Pics/small"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$files = Get-ChildItem -Path $dir -Filter "*.png"
foreach ($file in $files) {
    Write-Host "Resizing $($file.Name)..."
    $img = [System.Drawing.Image]::FromFile($file.FullName)
    $ratio = $img.Width / $img.Height
    $newW = 1024
    $newH = [int]($newW / $ratio)
    $newImg = New-Object System.Drawing.Bitmap($newW, $newH)
    $g = [System.Drawing.Graphics]::FromImage($newImg)
    $g.DrawImage($img, 0, 0, $newW, $newH)
    $outPath = Join-Path $outDir ($file.BaseName + ".jpg")
    $newImg.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Jpeg)
    $g.Dispose()
    $newImg.Dispose()
    $img.Dispose()
}
Write-Host "Done!"
