@echo off
avrdude -p t402 -c serialupdi -P COM16 -U fuse0:r:-:h -U fuse2:r:-:h -U fuse5:r:-:h -U fuse6:r:-:h -U fuse8:r:-:h
