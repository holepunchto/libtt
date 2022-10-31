process.stdin.resume()

process.stdout.on('resize', () => {
  console.log(`${process.stdout.columns}x${process.stdout.rows}`)
})
