{
  'targets': [{
    'target_name': 'libtt',
    'type': 'static_library',
    'include_dirs': [
      './include',
    ],
    'sources': [
      './src/pty.c',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        './include',
      ],
    },
    'configurations': {
      'Debug': {
        'defines': ['DEBUG'],
      },
      'Release': {
        'defines': ['NDEBUG'],
      },
    },
    'conditions': [
      ['OS=="win"', {
        'sources': [
          './src/win/pty.c',
        ],
      }, {
        'sources': [
          './src/unix/pty.c',
        ],
      }],
    ],
  }]
}
