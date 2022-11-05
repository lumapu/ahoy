# Information

This repository contains the sources for https://ahoydtu.de. The sources are converted by a *static site generator* [Hugo](https://gohugo.io/).
The main Repository URL is: https://github.com/lumapu/ahoy


# Contribute

Feel free to modify / add contents of the website which helps other users to understand better how to start or use AhoyDTU.

## Quick Howto

* edit files inside the directory `content` most of them are Markdown files (\*.md).
* once you're finish commit your changes (as Pull-Request)
* wait until a collaborator merges your changes

## Design / CSS

* this site uses [Bootstrap 5](https://getbootstrap.com/docs/5.2/getting-started/introduction/)

## Offline convertion of sources to website

### Docker

* you can use a local docker container which runs Hugo to convert the sources to a static site:

  `docker run --rm -it -v ${pwd}\:/src -p 80:1313 klakegg/hugo:0.101.0-ext-alpine shell`
* from the shell start hugo server by typing `hugo server`
* point your browser to http://localhost

### Hokus

Hokus is a WYSIWYG Hugo Editor [](https://www.hokuscms.com/)
