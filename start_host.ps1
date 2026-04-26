$wslIP = (wsl hostname -I).Trim().Split(" ")[0]
netsh interface portproxy delete v4tov4 listenport=27015 2>$null
netsh interface portproxy add v4tov4 listenport=27015 listenaddress=0.0.0.0 connectport=27015 connectaddress=$wslIP
netsh advfirewall firewall add rule name="Battleship" dir=in action=allow protocol=TCP localport=27015 2>$null
Write-Host "Ready. Your IP is:"
(Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.InterfaceAlias -like "Ethernet*" }).IPAddress
Read-Host "Press Enter to close"