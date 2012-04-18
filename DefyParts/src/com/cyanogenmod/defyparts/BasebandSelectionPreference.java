package com.cyanogenmod.defyparts;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.SystemProperties;
import android.preference.Preference;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.SpinnerAdapter;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class BasebandSelectionPreference extends Preference implements Preference.OnPreferenceClickListener
{
    private static final String TAG = "BasebandSelectionPreference";

    private static final String PROP_NAME = "persist.sys.baseband";
    private static final String DEFAULT_BASEBAND = "europe:ce:3.4.x";

    private static class RegionInfo {
        String name;
        String[] countryEntries;
        String[] countryValues;
        /* country name -> country info */
        Map<String, CountryInfo> countries;
    };

    private static class CountryInfo {
        String name;
        String uiName;
        String[] versions;
        /* version -> baseband */
        Map<String, BasebandInfo> basebands;
    };

    private static class BasebandInfo {
        String region;
        String country;
        String version;
        List<String> frequencies;
        String filename;
    };

    private List<BasebandInfo> mBasebands;
    /* region name -> region info */
    private Map<String, RegionInfo> mRegions;
    private String[] mRegionValues;
    private String[] mValue;
    private Resources mResources;

    public BasebandSelectionPreference(Context context, AttributeSet attrs) {
        super(context,attrs);

        mResources = getContext().getResources();
        buildBasebandList();
        buildRegionMap();
        initValue();
        setOnPreferenceClickListener(this);
    }

    @Override
    public boolean onPreferenceClick(Preference pref) {
        final RegionInfo region = mRegions.get(mValue[0]);
        final CountryInfo country = region.countries.get(mValue[1]);
        int regionPos = -1, countryPos = -1, versionPos = -1;

        for (int i = 0; i < mRegionValues.length; i++) {
            if (mValue[0].equals(mRegionValues[i])) {
                regionPos = i;
                break;
            }
        }
        for (int i = 0; i < region.countryValues.length; i++) {
            if (mValue[1].equals(region.countryValues[i])) {
                countryPos = i;
                break;
            }
        }
        for (int i = 0; i < country.versions.length; i++) {
            if (mValue[2].equals(country.versions[i])) {
                versionPos = i;
                break;
            }
        }

        Dialog d = new SelectionDialog(getContext(), regionPos, countryPos, versionPos);
        d.show();

        return true;
    }

    private void initValue() {
        final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(getContext());
        final String basebandString = prefs.getString(getKey(), DEFAULT_BASEBAND);

        mValue = TextUtils.split(basebandString, ":");

        BasebandInfo baseband = null;
        if (mValue.length == 3) {
            baseband = getBaseband(mValue[0], mValue[1], mValue[2]);
        }
        if (baseband == null) {
            mValue = TextUtils.split(DEFAULT_BASEBAND, ":");
            updateValue(mValue[0], mValue[1], mValue[2]);
        }

        updateSummary();
    }

    private BasebandInfo getBaseband(final String region, final String country, final String version) {
        final RegionInfo regionInfo = mRegions.get(region);
        if (regionInfo != null) {
            final CountryInfo countryInfo = regionInfo.countries.get(country);
            if (countryInfo != null) {
                return countryInfo.basebands.get(version);
            }
        }
        return null;
    }

    private void updateValue(final String region, final String country, final String version) {
        final SharedPreferences.Editor editor =
                PreferenceManager.getDefaultSharedPreferences(getContext()).edit();
        final StringBuilder builder = new StringBuilder();
        final BasebandInfo baseband = getBaseband(region, country, version);

        if (baseband == null) {
            return;
        }

        builder.append(region);
        builder.append(':');
        builder.append(country);
        builder.append(':');
        builder.append(version);

        final String prefValue = builder.toString();
        editor.putString(getKey(), prefValue);
        editor.commit();

        mValue[0] = region;
        mValue[1] = country;
        mValue[2] = version;

        SystemProperties.set(PROP_NAME, baseband.filename);
        Log.i(TAG, "Updated baseband to " + prefValue + ", filename " + baseband.filename);
        updateSummary();
        callChangeListener(this);
    }

    private void updateSummary() {
        final RegionInfo region = mRegions.get(mValue[0]);
        final CountryInfo country = region.countries.get(mValue[1]);
        final BasebandInfo baseband = country.basebands.get(mValue[2]);

        setSummary(mResources.getString(R.string.baseband_selection_summary, country.uiName, baseband.version));
    }

    private void buildBasebandList() {
        final Resources res = getContext().getResources();
        mBasebands = new ArrayList<BasebandInfo>();

        for (String item : res.getStringArray(R.array.available_basebands)) {
            String[] values = TextUtils.split(item, ":");
            if (values.length != 5) {
                Log.w(TAG, "Ignoring invalid baseband specification " + item);
                continue;
            }

            final BasebandInfo info = new BasebandInfo();
            info.region = values[0];
            info.country = values[1];
            info.version = values[2];
            info.frequencies = new ArrayList<String>();
            info.filename = values[4];

            for (final String frequency : TextUtils.split(values[3], ",")) {
                info.frequencies.add(frequency);
            }

            mBasebands.add(info);
        }
    }

    private void buildRegionMap() {
        final Resources res = getContext().getResources();
        final String packageName = getClass().getPackage().getName();

        mRegionValues = res.getStringArray(R.array.baseband_region_values);
        mRegions = new HashMap<String, RegionInfo>();

        for (int rIndex = 0; rIndex < mRegionValues.length; rIndex++) {
            final String regionName = mRegionValues[rIndex];
            final String entryName = "baseband_countries_" + regionName + "_entries";
            final String valueName = "baseband_countries_" + regionName + "_values";
            final RegionInfo region = new RegionInfo();

            final int entryResId = res.getIdentifier(entryName, "array", packageName);
            final int valueResId = res.getIdentifier(valueName, "array", packageName);

            region.name = regionName;
            region.countries = new HashMap<String, CountryInfo>();

            if (entryResId != 0 && valueResId != 0) {
                region.countryEntries = res.getStringArray(entryResId);
                region.countryValues = res.getStringArray(valueResId);

                for (int cIndex = 0; cIndex < region.countryValues.length; cIndex++) {
                    final String countryName = region.countryValues[cIndex];
                    final CountryInfo country = new CountryInfo();

                    country.name = countryName;
                    country.uiName = region.countryEntries[cIndex];
                    country.basebands = new HashMap<String, BasebandInfo>();

                    for (final BasebandInfo baseband : mBasebands) {
                        if (regionName.equals(baseband.region) && countryName.equals(baseband.country)) {
                            country.basebands.put(baseband.version, baseband);
                        }
                    }
                    final Set<String> versions = country.basebands.keySet();
                    country.versions = versions.toArray(new String[versions.size()]);

                    region.countries.put(countryName, country);
                }
            }

            mRegions.put(regionName, region);
        }
    }

    private class SelectionDialog extends AlertDialog implements AdapterView.OnItemSelectedListener {
        private Spinner mRegion;
        private Spinner mCountry;
        private Spinner mVersion;
        private TextView mFrequencies;

        private int[] mInitialPositions;
        private String mSelectedRegion;
        private String mSelectedCountry;
        private String mSelectedVersion;

        public SelectionDialog(Context context, int regionPos, int countryPos, int versionPos) {
            super(context);
            mInitialPositions = new int[] { regionPos, countryPos, versionPos };
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            initViews();
            initButtons();

            super.onCreate(savedInstanceState);

            mRegion.setSelection(mInitialPositions[0]);
            updateCountrySpinner();
            mCountry.setSelection(mInitialPositions[1]);
            updateVersionSpinner();
            mVersion.setSelection(mInitialPositions[2]);
            updateFrequencies();
        }

        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            if (parent == mRegion) {
                updateCountrySpinner();
            } else if (parent == mCountry) {
                updateVersionSpinner();
            }
            updateFrequencies();
            validate();
        }

        @Override
        public void onNothingSelected(AdapterView parent) {
            if (parent == mRegion) {
                updateCountrySpinner();
            } else if (parent == mCountry) {
                updateVersionSpinner();
            }
            updateFrequencies();
            validate();
        }

        private void initViews() {
            final View layout = getLayoutInflater().inflate(R.layout.dialog_baseband_selection, null);

            mRegion = (Spinner) layout.findViewById(R.id.region);
            mRegion.setOnItemSelectedListener(this);
            mCountry = (Spinner) layout.findViewById(R.id.country);
            mCountry.setOnItemSelectedListener(this);
            mCountry.setAdapter(new SpinnerUpdateAdapter(getContext()));
            mVersion = (Spinner) layout.findViewById(R.id.baseband);
            mVersion.setOnItemSelectedListener(this);
            mVersion.setAdapter(new SpinnerUpdateAdapter(getContext()));
            mFrequencies = (TextView) layout.findViewById(R.id.frequencies);

            setView(layout);
            setTitle(R.string.baseband_switcher);
        }

        private void initButtons() {
            setButton(BUTTON_POSITIVE, mResources.getString(android.R.string.ok), new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    updateValue(getSelectedRegion().name, getSelectedCountry().name, getSelectedBaseband().version);
                }
            });
            setButton(BUTTON_NEGATIVE, mResources.getString(android.R.string.cancel), (DialogInterface.OnClickListener) null);
        }

        private void updateCountrySpinner() {
            final RegionInfo region = getSelectedRegion();
            String[] countries = region != null ? region.countryEntries : null;

            updateSpinner(mCountry, countries);
            updateVersionSpinner();
            validate();
        }

        private void updateVersionSpinner() {
            final RegionInfo region = mSelectedRegion != null ? mRegions.get(mSelectedRegion) : null;
            final CountryInfo country = getSelectedCountry();
            String[] versions = country != null ? country.versions : null;

            updateSpinner(mVersion, versions);
            validate();
        }

        private void validate() {
            boolean valid = getSelectedBaseband() != null;
            getButton(BUTTON_POSITIVE).setEnabled(valid);
        }

        private void updateSpinner(Spinner spinner, String[] items) {
            if (items == null) {
                items = new String[0];
            }

            SpinnerUpdateAdapter adapter = (SpinnerUpdateAdapter) spinner.getAdapter();
            adapter.update(items);
            spinner.setEnabled(items.length > 0);
        }

        private void updateFrequencies() {
            BasebandInfo info = getSelectedBaseband();
            if (info != null) {
                final String freqList = TextUtils.join("/", info.frequencies);
                mFrequencies.setText(mResources.getString(R.string.baseband_frequencies, freqList));
                mFrequencies.setVisibility(View.VISIBLE);
            } else {
                mFrequencies.setVisibility(View.GONE);
            }
        }

        private RegionInfo getSelectedRegion() {
            int pos = mRegion.getSelectedItemPosition();
            if (pos == AdapterView.INVALID_POSITION || pos >= mRegion.getCount()) {
                return null;
            }
            return mRegions.get(mRegionValues[pos]);
        }

        private CountryInfo getSelectedCountry() {
            int pos = mCountry.getSelectedItemPosition();
            final RegionInfo region = getSelectedRegion();

            if (region == null || pos >= mCountry.getCount() || pos == AdapterView.INVALID_POSITION) {
                return null;
            }
            return region.countries.get(region.countryValues[pos]);
        }

        private BasebandInfo getSelectedBaseband() {
            int pos = mVersion.getSelectedItemPosition();
            final CountryInfo country = getSelectedCountry();

            if (country == null || pos >= mVersion.getCount() || pos == AdapterView.INVALID_POSITION) {
                return null;
            }
            return country.basebands.get(country.versions[pos]);
        }

        private class SpinnerUpdateAdapter extends ArrayAdapter<CharSequence> {
            public SpinnerUpdateAdapter(Context context) {
                super(context, android.R.layout.simple_spinner_item, new ArrayList<CharSequence>());
                setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
                setNotifyOnChange(false);
            }

            public void update(String[] items) {
                clear();
                for (String item : items) {
                    add(item);
                }
                notifyDataSetChanged();
            }
        };
    };
}

