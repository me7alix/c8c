# c8c

A high-level assembler for CHIP-8, written in C using [spc.h](https://github.com/me7alix/spc.h).

## Idea

The main goal of this project is to make writing CHIP-8 programs both enjoyable and give you full control over the machine.

## Language design

The syntax is C-like because why not. The `main` procedure is the entry point and must be present somewhere in the source code.

Example (fibonacci.c8):

```asm
alias len = 32
bytes arr = arrayOf(len)

proc main {
    alias a = v1
    alias b = v2
    alias l = v3

    a = 0
    b = 0

    ve = 0
    while ve < len {
        l = b
        b += a
        a = l

        arr[ve] = a
        ve += 1
    }
}
```

## Peculiarities

Many language constructs have side effects on the `v0` and `vf` registers, so I don't recommend using them as general-purpose registers.

* Any of these conditions (`>`, `<`, `>=`, `<=`) uses the `vf` register.
* Array access always uses the `v0` register.
* Array access with an immediate index value also uses the `v0` register.

### `with` statement

The `with` statement helps you map array values to registers (or variables) and write back changes automatically.

```
with {
    v1 <-> blt_x [ve] as x
    v2 <-> blt_y [ve] as y
    v3 <-> blt_dx[ve] as dx
    v4 <-> blt_dy[ve] as dy
} {
    if   y == 0  { dy = 2 }
    elif y >= 30 { dy = 0 }
    if   x == 0  { dx = 2 }
    elif x >= 62 { dx = 0 }

    if   dx == 0 { x -= 1 }
    elif dx == 2 { x += 1 }
    if   dy == 0 { y -= 1 }
    elif dy == 2 { y += 1 }
}
```
