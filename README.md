# maxtime

Run a Un*x command with a timeout

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




