// src/components/SyntaxHighlighterComponent.tsx
import React from 'react';
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter';
import {
  vs,
  vscDarkPlus,
} from 'react-syntax-highlighter/dist/esm/styles/prism';
import { useTheme } from '@mui/material/styles';

interface SyntaxHighlighterComponentProps {
  config: string;
}

const SyntaxHighlighterComponent: React.FC<SyntaxHighlighterComponentProps> = ({
  config,
}) => {
  const theme = useTheme();
  const syntaxHighlighterStyle =
    theme.palette.mode === 'dark' ? vscDarkPlus : vs;
  return (
    <SyntaxHighlighter language="yaml" style={syntaxHighlighterStyle}>
      {config}
    </SyntaxHighlighter>
  );
};

export default React.memo(SyntaxHighlighterComponent);
