# Sound files

Put WAV files here and upload them with `pio run -t uploadfs`. A file named
after one of the firmware's sounds replaces that sound's synthesized babble:

`chatter.wav` `happy.wav` `giggle.wav` `yawn.wav` `whoa.wav` `purr.wav`
`yum.wav` `hello.wav`

Any sound without a file is synthesized, so this folder can stay empty.

Format: PCM WAV, 8- or 16-bit, mono or stereo. Any sample rate works, but
16 kHz mono 16-bit matches the output and saves flash. To convert:

```
ffmpeg -i input.mp3 -ac 1 -ar 16000 -sample_fmt s16 hello.wav
```

The default partition table leaves about 1.4 MB for files, which is roughly
45 seconds of 16 kHz mono audio. The console's `play /sounds/<file>.wav`
plays any other file you upload.

The firmware ignores files that aren't `.wav` (this README included), but
they still take up flash.
