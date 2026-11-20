if exists("b:current_syntax")
    finish
endif

syntax region c8Comment start=/%/ end=/%/
    \ contains=@Spell

" Keywords
syntax keyword c8Keyword proc bytes words alias arrayOf macro rand with as bcd char
syntax keyword c8Conditional if elif else
syntax keyword c8Repeat while loop
syntax keyword c8Statement call cls draw drawi

" Registers
syntax match c8Register /\<v[0-8]\>/
syntax keyword c8Register i dt st kd

" Numbers
syntax match c8Hex /\<0x[0-9a-fA-F]\+\>/
syntax match c8Binary /\<0b[01]\+\>/
syntax match c8Number /\<\d\+\>/

" Operators
syntax match c8Operator /==\|!=\|+=\|-=\|>=\|<=/
syntax match c8Operator /[=+*\/<>-]/

" Procedure declarations
syntax match c8ProcName /\<proc\s\+\zs[a-zA-Z_][a-zA-Z0-9_]*/

" Alias declarations
syntax match c8AliasName /\<alias\s\+\zs[a-zA-Z_][a-zA-Z0-9_]*/

" Function-like identifiers
syntax match c8Identifier /\<[a-zA-Z_][a-zA-Z0-9_]*\>/

" Highlighting
highlight default link c8Comment Comment
highlight default link c8Keyword Keyword
highlight default link c8Conditional Conditional
highlight default link c8Repeat Repeat
highlight default link c8Statement Statement
highlight default link c8Register Identifier
highlight default link c8Binary Number
highlight default link c8Number Number
highlight default link c8Operator Operator
highlight default link c8ProcName Function
highlight default link c8AliasName Identifier
highlight default link c8Identifier Identifier
highlight default link c8Hex Number

syntax sync fromstart
let b:current_syntax = "c8"
