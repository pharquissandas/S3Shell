# Shell Implementation

## Software Systems Assignment 1 - Preet Harquissandas and Mikhail Agakov


---

## 1. Overview

s3 is a custom Unix-like shell implemented in C.
It supports execution of external programs, built-in commands, redirection, pipelines, batched commands, and optional extensions such as subshells.

This README describes the features implemented, architectural decisions, and any additional enhancements beyond the base requirements.

---

## 2. Features Implemented

### *2.1 Basic Command Execution*

✔ Parsing of command input using strtok

✔ Execution of external programs using fork() + execvp()

✔ Parent waits using wait() / custom reap()

✔ Support for multiple arguments

---

## 3. Redirection Support

### **3.1 Output Redirection (> and >>)**

✔ Implemented using open() + dup2()

✔ > overwrites file

✔ >> appends to file

### **3.2 Input Redirection (<)**

✔ Implemented using open(O_RDONLY) + dup2()

### *3.3 Implementation Notes*

* Detection of redirection operator using a custom parser
* Launch handled through launch_program_with_redirection()
* Only one redirection operator is required by the spec, and handling is limited to that by design

---

## 4. Built-in Command: cd

### *4.1 Features*

✔ Implemented directly in parent process via chdir()

### *4.2 Prompt Updating*

✔ Prompt displays current working directory

✔ Uses getcwd() inside construct_shell_prompt()

Example prompt:
`[/home/user/projects/s3 s3]$`

---

## 5. Pipelining (|)

### *5.1 Multi-Stage Pipelines*

✔ Full support for pipelines of arbitrary length

✔ Implemented using:

* pipe()
* dup2() in each child
* Multiple fork() calls in a loop

### *5.3 Handling Redirection Inside Pipelines*

✔ Redirection allowed on pipeline endpoints

---

## 6. Batched Commands (;)

### *6.1 Features*

✔ Splits command line into batch units

✔ Each batch unit processed independently

✔ Supports pipelines or plain commands inside batch elements

---

## 7. Proposed Extensions (PEs)

### *7.1 PE1: Subshells*

✔ Support for grouped commands in parentheses

✔ Each subshell executes in its own process environment

✔ Allows redirection & pipelines involving subshells

### *7.2 PE2: Nested Subshells*

✔ Recursive parsing of nested parentheses

✔ Stack-based matching for ( and )

---

## *8. Further Enhancements*

### 8.1 FE: Combined Input + Output Redirection

✔ Support for redirecting both standard input and standard output in a single command

✔ Shell parses and records all redirection operators before executing the command

✔ Input and output redirections are applied by configuring the appropriate file descriptors

✔ Compatible with other features such as pipelines and subshell execution

✔ Combined input + output redirection in a single command

---

## 9. Testing Summary

We used a set of commands covering all features:

### *Basic Commands*

* `ls`
* `wc txt/phrases.txt`
* `man cat`
* `grep burn txt/phrases.txt`

### *Redirection*

* `grep June < calendar.txt`
* `tr a-z A-Z < phrases.txt`

### *Pipelines*

* `cat txt/phrases.txt | sort | wc -l`
* `ps aux | grep python | sort -k 3 -nr | head`
* `tr a-z A-Z < txt/phrases.txt | grep BURN`

### *Batched Commands*

* `echo A ; echo B ; ls -l`
* `mkdir results ; cat txt/phrases.txt | sort > results/sorted_phrases.txt ; echo "Done"`

### *Subshells*

* `(cd txt ; ls)`
* `(cat txt/phrases.txt | sort) | head`
* Nested: `((echo A))`
* `(echo a ; (echo b ; (echo c)))`

### *Combined input + output redirection in a single command*

* `(tr a-z A-Z < txt/phrases.txt) > txt/phrases_upper.txt`
* `sort < unsorted.txt > sorted.txt`
* `((tr a-z A-Z < txt/phrases.txt) | head) > txt/new_file.txt`
---

## 10. Possible Future Enhancements

* Command history, arrow-key navigation
* Background jobs with process groups
* Globbing (*.txt, file??.c)
* More robust error messages

---

## 11. Compilation & Usage

### *Compile*

`gcc *.c -o s3`

`./s3`
