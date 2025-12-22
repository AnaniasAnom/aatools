# aatools

## chatlog
Tool to quickly keep diary or discussion files, by subject and date.

The files are in ~/chatlogs/{subject}/{YYYYMMDD}

#### edit a file for subject and date
    chatlog [date-spec] [subject]
#### add subject to the list of subjects
    chatlog create subject
#### list the dates present for a subject
    chatlog ls subject
#### list all subjects
    chatlog ls

date-spec can be a negative number (e.g. -3 for three days ago), or "yesterday", or MMDD, or YYYYMMDD, or if left out means today

subject is a string (no spaces)

If it is skipped then the last one used will be used again

You can pass the first few letters and it will open it, or give you a menu if there are less than ten matching

There is support for shell completion: in bash, "complete -C 'chatlog -complete' chatlog"
