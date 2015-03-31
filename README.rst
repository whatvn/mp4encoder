mp4encoder
============
* THIS IS `WORK-IN-PROGRESS` project, please careful consider before using it.
* Video encoding and delivery which makes use of ffmpeg api (libav*), gearman and ftplib  

* Step
	1. n worker Receive job from gearman server
	2. convert video to mp4 standard
	3. deliver file to config ftp server


* mp4encoder will examine input video file, then decide to convert it into correct resolution. if video should be converted to 360p, it will be converted to 360p. 
* mp4encoder is optimized to work with ``eStreaming server``, our streaming server that automatically split video into small http live streaming ts file and playlist, with adaptive bitrate support.

Usage
=====

config.ini
-----------------
::

	[file]
	source = /data/upload
	destination = /data/converted


	[ftp]
	host = 127.0.0.1
	user = video
	password = password



	[gearman]
	host = 127.0.0.1
	port = 4730
	jobname = convert
	worker = 10

If ftp section is not configured, local file (``[file]]``) will be used


Installation
============

*See below for OS-specific preparations.*

This project use libav* from ffmpeg project, make sure you already installed libav* before install ``mp4encoder``
Install *mp4encoder* with:

::

    # make 

- Modify config.ini  

- Start ``./mp4encoder``

License
=======

This project is used behind our video streaming server ``eStreaming``, which do samething as unified streaming, nginx-plus, vowza server and it does ``better``
@Ethic Consultant 2014
`GPL <http://www.gnu.org/licenses/gpl-3.0.txt>`_
