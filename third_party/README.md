# Included libraries

- TinyXML2 11.0.0: https://github.com/leethomason/tinyxml2/tree/11.0.0
  XML parsing, zlib license. Original license retained in tinyxml2/LICENSE.txt.
- miniz 3.0.2: https://github.com/richgel999/miniz/tree/3.0.2
  ZIP reading/writing, MIT license. Original license retained in miniz/LICENSE.
  miniz_export.h is the local static-build replacement for the generated header.

Files downloaded from the named upstream tags. No network dependency at build time.
Book text import uses XML and ZIP libraries; it does not launch another reader app.
The installed Kindle /usr/bin/curl provides HTTPS with its certificate checks enabled.
