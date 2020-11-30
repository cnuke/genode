package main

import (
    "fmt"
	"strings"
	"runtime"
)

func main() {
	fmt.Println("cpu number", runtime.NumCPU())
    c0 := make(chan string)
    c1 := make(chan string)
    go sourceGopher(c0)
    go splitWords(c0, c1)
    printGopher(c1)
}

func sourceGopher(downstream chan string) {
    for _, v := range []string{"hello world", "a bad apple", "goodbye all"} {
        downstream <- v
    }
    close(downstream)
}

func splitWords(upstream, downstream chan string) {
    for v := range upstream {
        for _, word := range strings.Fields(v) {
            downstream <- word
        }
    }
    close(downstream)
}

func printGopher(upstream chan string) {
    for v := range upstream {
        fmt.Println(v)
    }
}

// package main

// import "fmt"

// func main() {
// 	println("main !")
// 	fmt.Printf("hello, world\n")
// }
