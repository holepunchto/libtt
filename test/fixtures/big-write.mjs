const { stdout } = process

stdout.write(Buffer.alloc(64 * 1024, 'a'))
