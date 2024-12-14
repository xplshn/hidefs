# (1) HideFS - A kernel module that allows hiding files

I was inspired by gobohide, but I didn't like the following:

1. Not a module, it is ALWAYS enabled
2. Uses Netlink, instead of just exposing a character device
3. Bugs out in 6.1+
4. The license

So I decided to start from scratch

# (2) HideFS - Interface:


