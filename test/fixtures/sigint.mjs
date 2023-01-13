import { once } from 'events'

const { stdin, stdout } = process

stdin.resume()

await once(process, 'SIGINT')

stdout.write('SIGINT\n')

stdin.pause()
