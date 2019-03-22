# maxtime

Run a Un*x command with a timeout.

`maxtime` makes it possible to start a Unix command with a time limit imposed on the running (wall clock) time.
If the command runs for too long, it will be killed with SIGTERM or SIGKILL.

## Build:

```
./configure
make all
```

## Example:

Fetch a network file, but time out after 10 seconds:

```
maxtime 10 wget https://raw.githubusercontent.com/dwyl/english-words/master/words.txt
```




