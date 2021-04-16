# Fatum

![image](https://user-images.githubusercontent.com/20361252/115049567-28889c00-9edb-11eb-9af4-d870ccd3f0fc.png)

A simple FAT16 image viewer written in C. With this tool you can explore folders, display files' contents (works similarly to "cat" command in Unix) and more.

# Compiling
```gcc fatum.c```

The application reads fat16.bin file automatically. If you want, you can replace the file or change filepath in fatum.c file in main() function (in the future I should include typing filename in arguments, but right now it's not supported).

# Commands
The app includes CLI that can be interacted with with supported commands:
```dir - shows current directory's contents. You can also give a dir name to show its contents.
     syntax: dir [directory-name]
cd - changes current directory.
     syntax: cd directory-name
pwd - displays current directory's full path.
cat - shows file's contents.
     syntax: cat file-name
get - saves file to the host's folder.
     syntax: get file-name
zip - gets 2 files and mixes its contents to a new file.
     syntax: zip file1-name file2-name output-file-name
rootinfo - prints root directory info.
spaceinfo - prints volume information.
fileinfo - prints file details.
     syntax: fileinfo file-name
```

This list can be also displayed inside an app with ``help`` command.

# Why Fatum was made?
To know more about FAT file system and its structure. It recognizes only FAT16 images, but it could be modified to support FAT12 or FAT32 image files.

