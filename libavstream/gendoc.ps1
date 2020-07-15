echo "Generating HTML documentation ..."
Push-Location -Path doc
doxygen
Pop-Location
