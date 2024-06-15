import React from 'react';
import { FormControlLabel, Checkbox } from '@mui/material';

type KeysTyped<V, T> = keyof {
  [K in keyof V as V[K] extends T ? K : never]: V[K];
};

type BooleanFieldProps<T extends object> = {
  value: T;
  prop: KeysTyped<T, boolean> & string;
  onChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  label: string;
};

const BooleanField = <T extends object>({
  value,
  prop,
  onChange,
  label,
}: BooleanFieldProps<T>) => (
  <FormControlLabel
    control={
      <Checkbox
        checked={value[prop] as boolean}
        onChange={onChange}
        name={prop}
      />
    }
    label={label}
  />
);

export default BooleanField;
