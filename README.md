[![CC BY-NC-SA 4.0][cc-by-nc-sa-shield]][cc-by-nc-sa]
This work is licensed under a
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License][cc-by-nc-sa].

[![CC BY-NC-SA 4.0][cc-by-nc-sa-image]][cc-by-nc-sa]

[cc-by-nc-sa]: https://creativecommons.org/licenses/by-nc-sa/4.0/deed.de
[cc-by-nc-sa-image]: https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png
[cc-by-nc-sa-shield]: https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg

[release-action-badge]: https://github.com/lumapu/ahoy/actions/workflows/compile_release.yml/badge.svg
[release-action-link]: https://github.com/lumapu/ahoy/actions/workflows/compile_release.yml

[dev-action-badge]: https://github.com/lumapu/ahoy/actions/workflows/compile_development.yml/badge.svg
[dev-action-link]: https://github.com/lumapu/ahoy/actions/workflows/compile_development.yml


THIS IS A FORK OF:


# 🖐 Ahoy!
![Logo](https://github.com/grindylow/ahoy/blob/main/doc/logo1_small.png?raw=true)



For details on the base project visit the Ahoy site.

This fork adds the following features:
### Added chart to Display128x64

### Added a History menu
This menu displays
- a bar-chart of the last 256 Total-Power values
- a bar chart of the last 256 Yield-Day values.
Note: The history of the values gets lost after rebbot!

### Changed WiFi reconnect behaviour
For me the original reconnect did not work. I always lost connection after some time.
Now it is more stable.

### Reduced frequency channels
Removed frequencies 2461, 2475MHz. Now the transfers are much more stable