import React from 'react';
import {
  FormControlLabel,
  Checkbox,
  FormControl,
  FormHelperText,
} from '@mui/material';

type KeysTyped<V, T> = keyof {
  [K in keyof V as V[K] extends T ? K : never]: V[K];
};

type BooleanFieldProps<T extends object> = {
  value: T;
  prop: KeysTyped<T, boolean> & string;
  onChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  label: string;
  helperText?: React.ReactNode | string;
  error?: boolean;
};

const BooleanField = <T extends object>({
  value,
  prop,
  onChange,
  label,
  helperText,
  error,
}: BooleanFieldProps<T>) => (
  <FormControl>
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
    {helperText && <FormHelperText error={error}>{helperText}</FormHelperText>}
  </FormControl>
);

export default BooleanField;
