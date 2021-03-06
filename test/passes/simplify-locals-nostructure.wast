(module
  (func $contrast ;; check for tee and structure sinking
    (local $x i32)
    (local $y i32)
    (local $z i32)
    (local $a i32)
    (local $b i32)
    (set_local $x (i32.const 1))
    (if (get_local $x) (nop))
    (if (get_local $x) (nop))
    (set_local $y (if (result i32) (i32.const 2) (i32.const 3) (i32.const 4)))
    (drop (get_local $y))
    (set_local $z (block (result i32) (i32.const 5)))
    (drop (get_local $z))
    (if (i32.const 6)
      (set_local $a (i32.const 7))
      (set_local $a (i32.const 8))
    )
    (drop (get_local $a))
    (block $val
      (if (i32.const 10)
        (block
          (set_local $b (i32.const 11))
          (br $val)
        )
      )
      (set_local $b (i32.const 12))
    )
    (drop (get_local $b))
  )
  (func $no-unreachable
    (local $x i32)
    (drop
      (tee_local $x
        (unreachable)
      )
    )
  )
)

