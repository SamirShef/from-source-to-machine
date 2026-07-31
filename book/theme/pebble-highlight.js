document.addEventListener('DOMContentLoaded', () => {
  if (typeof hljs === 'undefined') {
    return;
  }

  hljs.registerLanguage('pebble', function(hljs) {
    const KEYWORDS = {
      keyword: 'fn var const struct import extern if else for return',
      literal: 'true false nil',
      type: 'int8 int16 int32 int64 uint8 uint16 uint32 uint64 float32 float64 bool char string void'
    };

    const NUMBER_SUFFIX = '([iu](8|16|32|64)|f(32|64))?';

    return {
      name: 'Pebble',
      aliases: ['pebble'],
      keywords: KEYWORDS,
      contains: [
        hljs.C_LINE_COMMENT_MODE,

        {
          className: 'string',
          begin: '"', end: '"',
          contains: [hljs.BACKSLASH_ESCAPE]
        },

        {
          className: 'string',
          begin: "'", end: "'",
          contains: [hljs.BACKSLASH_ESCAPE]
        },

        {
          className: 'number',
          variants: [
            { begin: '\\b0x[0-9a-fA-F]+' + NUMBER_SUFFIX },
            { begin: '\\b\\d+(\\.\\d+)?' + NUMBER_SUFFIX }
          ],
          relevance: 0
        },

        {
          className: 'title',
          begin: /\b(?!(?:if|for|return|fn|struct|import|extern|var|const)\b)[a-zA-Z_][a-zA-Z0-9_]*(?=\s*\()/
        },

        {
          className: 'function',
          beginKeywords: 'fn',
          end: '(\\{|;|=)',
          excludeEnd: true,
          contains: [
            hljs.TITLE_MODE,
            {
              className: 'params',
              begin: '\\(', end: '\\)',
              keywords: KEYWORDS,
              contains: [
                hljs.C_LINE_COMMENT_MODE,
                { className: 'string', begin: '"', end: '"' }
              ]
            }
          ]
        },

        {
          className: 'title.class',
          beginKeywords: 'struct',
          end: '(\\{|;)',
          excludeEnd: true,
          contains: [hljs.TITLE_MODE]
        },

        {
          className: 'type',
          begin: '\\*+[a-zA-Z_][a-zA-Z0-9_]*'
        }
      ]
    };
  });

  const highlightFn = hljs.highlightElement || hljs.highlightBlock;

  if (typeof highlightFn === 'function') {
    document.querySelectorAll('code.language-pebble').forEach((block) => {
      highlightFn(block);
    });
  }
});
