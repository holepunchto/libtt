import { once } from 'events'

const { stdin, stdout } = process

stdin.resume()

await once(stdout, 'resize')

stdout.write(`${stdout.columns}x${stdout.rows}`)

stdin.pause()
