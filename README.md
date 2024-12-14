# (1) HideFS - A kernel module that allows hiding files

I was inspired by gobohide, but I didn't like the following about GoboHide:

1. Not a module, it is ALWAYS enabled
2. Uses Netlink, instead of just exposing a character device that can be interfaced with shell, C, or anything else
3. Bugs out in 6.1+
4. The license

So I decided to start from scratch

# (2) HideFS - Interface:
- /sys/kernel/hidefs/hide: write here the filepaths which you wish to hide, (exactly one per write, yeah I know, I should make it work with a space separated list...)
- /sys/kernel/hidefs/unhide: write here the filepaths which you wish to unhide, (exactly one per write, yeah I know, I should make it work with a space separated list...)
- /sys/kernel/hidefs/list: the list of currently hidden files

### W.I.P. (HELP WANTED)
- Currently. I've not worked out a way to actually HIDE the files without modifying the kernel's source code. I wish to hook the readdir family of funtions, as well as the filldir family of functions. However, these are UNEXPORTED, and we'd need to use a hook engine (which, 1; I do not know how to use, 2; I don't want to do hacky stuff, not more than this, 3; I don't want to use GPL unless completely unavoidable)

### Show & tell

![image](https://github.com/user-attachments/assets/7ff5b0ac-a77d-40b5-ab03-2e299aa61f52)
