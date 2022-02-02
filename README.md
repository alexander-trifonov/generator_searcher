# Generator & Searcher
## Description
Two threads communicate each other via shared container.

* First thread, the *Generator*, generates *Messages* and stores in a shared thread-safe *Container*:
```
Message's fields:
  phone_number - a string representing phone number in "+7-999-999-99-99" format
  login - some string representing user's login ("login_x" for example)
```

* Second thread, the *Searcher*, does the ranked search of newly arrived *Message* with others. If `phone_number` and `login` are the same, it's rank is 2; if only one field is equal, then 1. *Searcher* choose the *Message* with the highest rank and process it: deletes found Message. If there is no similar *Message*, then the *Message* will be added to the internal storage for further comparisons. Each added *Message* in the internal *Searcher* storage have a timed lifespan to not let the storage overflow

## Building:
`g++ --std=c17 main.cpp -o <file_output_name>`
