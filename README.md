# Qt Media Library Abstraction

This is a simple wrapper around a multimedia playback library. I use it in my apps at https://flavio.tordini.org

The most interesting and maintained backend is MPV. It works great on macOS, Windows and Linux.

I wrote this high level wrapper because I was not happy with Qt Multimedia and its lack of a common backend and guaranteed media format support across desktop platforms.

Define `MEDIA_QTAV` to link to QtAV or `MEDIA_MPV` to link to libmpv (>=0.29.0).

`MEDIA_AUDIOONLY` can be defined if the application does not need video.

You can use this library under the GPLv3 license. If you do, you're welcome contributing your changes and fixes.

If you would like to use this library in a commercial project, contact me at flavio.tordini@gmail.com