// src/components/SyntaxHighlighterComponent.tsx
import React from 'react';
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter';
import { vs } from 'react-syntax-highlighter/dist/esm/styles/prism';

interface SyntaxHighlighterComponentProps {
  config: string;
}

const SyntaxHighlighterComponent: React.FC<SyntaxHighlighterComponentProps> = ({
  config,
}) => {
  return (
    <SyntaxHighlighter language="yaml" style={vs}>
      {config}
    </SyntaxHighlighter>
  );
};

export default React.memo(SyntaxHighlighterComponent);
