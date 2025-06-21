tasklist /FI "IMAGENAME eq Spotify.exe" | find /I "Spotify.exe"
if %ERRORLEVEL%==0 (
    taskkill /F /IM Spotify.exe
) else (
    start spotify
)