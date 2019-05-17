[![Build Status](https://travis-ci.org/cboxdoerfer/fsearch.svg?branch=master)](https://travis-ci.org/cboxdoerfer/fsearch)
[![Translation status](https://hosted.weblate.org/widgets/fsearch/-/svg-badge.svg)](https://hosted.weblate.org/engage/fsearch/?utm_source=widget)

FSearch is a fast file search utility, inspired by Everything Search Engine. It's written in C and based on GTK+3.

**Note: The application is still in beta stage, but will see its first release as soon as localization support has been added**

![](https://i.imgur.com/LvsxlWD.png)

## Features
- Instant (as you type) results
- Wildcard support
- RegEx support
- Filter support (only search for files, folders or everything)
- Include and exclude specific folders to be indexed
- Ability to exclude certain files/folders from index using wildcard expressions
- Fast sort by filename, path, size or modification time
- Customizable interface

## Requirements
- GTK+ 3.12
- GLib 2.36
- glibc 2.19 or musl 1.1.15 (other C standard libraries might work too, those are just the ones I verified)
- PCRE (libpcre)

## Download
#### Arch Linux (AUR)
https://aur.archlinux.org/packages/fsearch-git/
#### openSUSE
https://software.opensuse.org/download.html?project=home%3AAsterPhoenix13&package=fsearch
#### Ubuntu
##### Daily Development Builds PPA
https://launchpad.net/~christian-boxdoerfer/+archive/ubuntu/fsearch-daily
 
## Roadmap
https://github.com/cboxdoerfer/fsearch/wiki/Roadmap

## Build Instructions
https://github.com/cboxdoerfer/fsearch/wiki/Build-instructions

## Localization
The localization of FSearch is managed with Weblate. 

https://hosted.weblate.org/projects/fsearch/

If you want to contribute translations please submit them there, instead of opening pull requets on Github. Instructions can be found here: 
https://weblate.readthedocs.io/en/latest/user/index.html

## Why yet another search utility?
Performance. On Windows I really like to use Everything Search Engine. It provides instant results as you type for all your files and lots of useful features (regex, filters, bookmarks, ...). On Linux however I couldn't find anything that's even remotely as fast and powerful.

Before I started working on FSearch I took a look at all the existing solutions I found (MATE Search Tool (formerly GNOME Search Tool), Recoll, Krusader (locate based search), SpaceFM File Search, Nautilus, ANGRYsearch, Catfish, ...) to find out whether it makes sense to improve those, instead of building a completely new application. But unfortunately none of those met my requirements:
- standalone application (not part of a file manager)
- written in a language with C like performance
- no dependencies to any specific desktop environment
- Qt5 or GTK+3 based
- small memory usage (both hard drive and RAM)
- target audience: advanced users

## Looking for a command line interface?
I highly recommend [fzf](https://github.com/junegunn/fzf) or the obvious tools: find and (m)locate

## Why GTK+3 and not Qt5?
I like both of them. And in fact my long term goal is to provide console, GTK+3 and Qt5 interfaces, or at least make it possible for others to build those by splitting the search and database functionality into a core library. But for the time being it's only GTK+3 because I tend to like C more than C++ and I'm more familiar with GTK+ development.

## Questions?

Email: christian.boxdoerfer[AT]posteo.de
