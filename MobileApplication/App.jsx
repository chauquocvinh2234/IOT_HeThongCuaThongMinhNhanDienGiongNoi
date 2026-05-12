import './global.css';
import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { Ionicons } from '@expo/vector-icons';
import { StatusBar } from 'expo-status-bar';
import { View } from 'react-native';
import { SafeAreaProvider, useSafeAreaInsets } from 'react-native-safe-area-context';

import DashboardScreen from './src/screens/DashboardScreen.jsx';
import HistoryScreen from './src/screens/HistoryScreen.jsx';
import SettingsScreen from './src/screens/SettingsScreen.jsx';
import LoginScreen from './src/screens/LoginScreen.jsx';
import RegisterScreen from './src/screens/RegisterScreen.jsx';
import { AuthProvider } from './src/contexts/AuthContext.jsx';

const Tab = createBottomTabNavigator();
const Stack = createNativeStackNavigator();

function DashboardTabIcon({ color, size }) {
  return <Ionicons name="grid-outline" size={size} color={color} />;
}

function HistoryTabIcon({ color, size }) {
  return <Ionicons name="time-outline" size={size} color={color} />;
}

function SettingsTabIcon({ color, size }) {
  return <Ionicons name="settings-outline" size={size} color={color} />;
}

function MainTabs() {
  const insets = useSafeAreaInsets();
  return (
    <Tab.Navigator
      screenOptions={{
        headerShown: false,
        tabBarActiveTintColor: '#6C63FF',
        tabBarInactiveTintColor: '#5A5A7A',
        tabBarStyle: {
          backgroundColor: '#16162A',
          borderTopColor: '#2A2A4A',
          borderTopWidth: 1,
          height: 65 + insets.bottom,
          paddingBottom: insets.bottom,
          paddingTop: 8,
        },
        tabBarLabelStyle: {
          fontSize: 12,
          fontWeight: '600',
        },
      }}
    >
      <Tab.Screen
        name="Dashboard"
        component={DashboardScreen}
        options={{
          tabBarLabel: 'Trang chủ',
          tabBarIcon: DashboardTabIcon,
        }}
      />
      <Tab.Screen
        name="History"
        component={HistoryScreen}
        options={{
          tabBarLabel: 'Lịch sử',
          tabBarIcon: HistoryTabIcon,
        }}
      />
      <Tab.Screen
        name="Settings"
        component={SettingsScreen}
        options={{
          tabBarLabel: 'Cài đặt',
          tabBarIcon: SettingsTabIcon,
        }}
      />
    </Tab.Navigator>
  );
}

export default function App() {
  return (
    <AuthProvider>
      <SafeAreaProvider>
        <View style={{ flex: 1, backgroundColor: '#0F0F1A' }}>
          <NavigationContainer
            theme={{
              dark: true,
              colors: {
                primary: '#6C63FF',
                background: '#0F0F1A',
                card: '#1A1A2E',
                text: '#FFFFFF',
                border: '#2A2A4A',
                notification: '#6C63FF',
              },
              fonts: {
                regular: { fontFamily: 'System', fontWeight: '400' },
                medium: { fontFamily: 'System', fontWeight: '500' },
                bold: { fontFamily: 'System', fontWeight: '700' },
                heavy: { fontFamily: 'System', fontWeight: '800' },
              },
            }}
          >
            <StatusBar style="light" />
            <Stack.Navigator 
              initialRouteName="Login"
              screenOptions={{ 
                headerShown: false,
                contentStyle: { backgroundColor: '#0F0F1A' },
                animation: 'none'
              }}
            >
              <Stack.Screen name="Login" component={LoginScreen} />
              <Stack.Screen name="Register" component={RegisterScreen} />
              <Stack.Screen name="Main" component={MainTabs} />
            </Stack.Navigator>
          </NavigationContainer>
        </View>
      </SafeAreaProvider>
    </AuthProvider>
  );
}
