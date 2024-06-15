import { FormValues } from '../types';
import React from 'react';

export const makeHandleInputChange = (formValues: FormValues, onFormChange: (data: FormValues) => void) =>
    (name: keyof FormValues & string) => (e: React.ChangeEvent<HTMLInputElement>) => {
        const { value, type, checked, name: inputName } = e.target;
        onFormChange({
            ...formValues,
            [name]: {
                ...formValues[name] as any,
                [inputName]: type === 'checkbox' ? checked : value,
            }
        });
    };
