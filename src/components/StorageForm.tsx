// src/components/StorageForm.tsx
import React from 'react';
import { Button, TextField, Box, Typography, Grid } from '@mui/material';
import { Storage } from '../types';

interface StorageFormProps {
  storages: Storage[];
  onChange: (storages: Storage[]) => void;
  maxStorages: number;
  minStorages: number;
}

const StorageForm: React.FC<StorageFormProps> = ({
  storages,
  onChange,
  maxStorages,
  minStorages,
}) => {
  const handleStorageChange = (
    index: number,
    key: string,
    value: string | number
  ) => {
    const newStorages = [...storages];
    newStorages[index] = { ...newStorages[index], [key]: value };
    onChange(newStorages);
  };

  const handleAddStorage = () => {
    if (storages.length < maxStorages) {
      onChange([...storages, { name: '', version: 1, mac_address: '' }]);
    }
  };

  const handleRemoveStorage = (index: number) => {
    if (storages.length > minStorages) {
      const newStorages = storages.filter((_, i) => i !== index);
      onChange(newStorages);
    }
  };

  const formatMacAddress = (value: string): string => {
    // Remove all non-hex characters
    const hexOnly = value.replace(/[^a-fA-F0-9]/g, '');
    // Insert colons after every two hex digits
    return (
      hexOnly
        .match(/.{1,2}/g)
        ?.join(':')
        .slice(0, 17) ?? ''
    );
  };

  const handleMacAddressChange = (index: number, value: string) => {
    const formattedValue = formatMacAddress(value);
    handleStorageChange(index, 'mac_address', formattedValue);
  };

  const handleMacAddressKeyDown =
    (index: number) => (e: React.KeyboardEvent<HTMLInputElement>) => {
      if (e.key === 'Backspace') {
        const target = e.target as HTMLInputElement;
        const value = target.value;
        const selectionStart = target.selectionStart || 0;

        // If the cursor is right after a colon and Backspace is pressed, remove the colon and the previous character
        if (selectionStart > 0 && value[selectionStart - 1] === ':') {
          e.preventDefault();
          const newValue =
            value.slice(0, selectionStart - 2) + value.slice(selectionStart);
          handleMacAddressChange(index, newValue);
        }
      }
    };

  return (
    <Box mt={2}>
      <Typography variant="h6">Storages</Typography>
      {storages.map((storage, index) => {
        let nameValid = storage.name.trim() === '';
        let versionValid =
          !storage.version || storage.version < 1 || storage.version > 2;
        let macValid = !/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(
          storage.mac_address
        );
        return (
          <Box key={index} mb={2} border={1} borderRadius={5} padding={2}>
            <Typography variant="subtitle1">{index + 1}. Storage</Typography>
            <Grid container spacing={2}>
              <Grid item xs={4}>
                <TextField
                  label="Name"
                  value={storage.name}
                  onChange={(e) =>
                    handleStorageChange(index, 'name', e.target.value)
                  }
                  fullWidth
                  margin="normal"
                  required
                  error={nameValid}
                  helperText={nameValid ? 'Name is required' : ''}
                />
              </Grid>
              <Grid item xs={4}>
                <TextField
                  label="Version"
                  type="number"
                  value={storage.version}
                  onChange={(e) =>
                    handleStorageChange(
                      index,
                      'version',
                      parseInt(e.target.value, 10)
                    )
                  }
                  fullWidth
                  margin="normal"
                  inputProps={{ min: 1, max: 2 }}
                  required
                  error={versionValid}
                  helperText={'Version must be 1 or 2'}
                />
              </Grid>
              <Grid item xs={4}>
                <TextField
                  label="MAC Address"
                  value={storage.mac_address}
                  onChange={(e) =>
                    handleMacAddressChange(index, e.target.value)
                  }
                  onKeyDown={handleMacAddressKeyDown(index)}
                  fullWidth
                  margin="normal"
                  error={macValid}
                  required
                  helperText="You can find the MAC address in the PowerZero app by pressing on the device name on the top left corner of the screen."
                />
              </Grid>
              <Grid item xs={12}>
                <Button
                  variant="contained"
                  color="secondary"
                  onClick={() => handleRemoveStorage(index)}
                  disabled={storages.length <= minStorages}
                >
                  Remove
                </Button>
              </Grid>
            </Grid>
          </Box>
        );
      })}
      {storages.length < maxStorages && (
        <Button variant="contained" color="primary" onClick={handleAddStorage}>
          Add Storage
        </Button>
      )}
    </Box>
  );
};

export { StorageForm };
