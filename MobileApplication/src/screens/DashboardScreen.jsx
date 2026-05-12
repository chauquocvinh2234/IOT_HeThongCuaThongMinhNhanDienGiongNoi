import React, { useRef, useEffect } from 'react';
import {
  View,
  Text,
  Animated,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { SafeAreaView } from 'react-native-safe-area-context';
import { useAuth } from '../contexts/AuthContext';

const COLORS = {
  primary: '#6C63FF',
  primaryLight: '#8B83FF',
  secondary: '#00D9A6',
  background: '#0F0F1A',
  card: '#1A1A2E',
  text: '#FFFFFF',
  textSecondary: '#8A8AA3',
  border: '#2A2A4A',
};

export default function DashboardScreen() {
  const { user } = useAuth();
  const fadeAnim = useRef(new Animated.Value(0)).current;
  const slideAnim = useRef(new Animated.Value(40)).current;
  const scaleAnim = useRef(new Animated.Value(0.8)).current;

  useEffect(() => {
    Animated.parallel([
      Animated.timing(fadeAnim, {
        toValue: 1,
        duration: 1000,
        useNativeDriver: true,
      }),
      Animated.spring(slideAnim, {
        toValue: 0,
        tension: 50,
        friction: 8,
        useNativeDriver: true,
      }),
      Animated.spring(scaleAnim, {
        toValue: 1,
        tension: 50,
        friction: 6,
        useNativeDriver: true,
      }),
    ]).start();
  }, []);

  const displayName = user?.fullname || 'Người dùng';

  return (
    <SafeAreaView className="flex-1 bg-background">
      <View className="flex-1 justify-center items-center px-8">

        {/* Animated Welcome Icon */}
        <Animated.View
          style={{
            opacity: fadeAnim,
            transform: [{ scale: scaleAnim }],
          }}
        >
          <View
            className="w-28 h-28 rounded-full justify-center items-center mb-8"
            style={{ backgroundColor: COLORS.primary + '20' }}
          >
            <View
              className="w-20 h-20 rounded-full justify-center items-center"
              style={{ backgroundColor: COLORS.primary + '35' }}
            >
              <Ionicons name="home" size={40} color={COLORS.primary} />
            </View>
          </View>
        </Animated.View>

        {/* Welcome Text */}
        <Animated.View
          style={{
            opacity: fadeAnim,
            transform: [{ translateY: slideAnim }],
            alignItems: 'center',
          }}
        >
          <Text className="text-base text-textSecondary mb-2">
            Xin chào 👋
          </Text>
          <Text className="text-3xl font-bold text-text text-center mb-3">
            Chào mừng{'\n'}
            <Text style={{ color: COLORS.primary }}>{displayName}</Text>
            {'\n'}quay trở lại!
          </Text>
          <Text className="text-sm text-textSecondary text-center mt-2 leading-5">
            Hệ thống Nhà thông minh đang hoạt động bình thường
          </Text>
        </Animated.View>

        {/* Status Indicator */}
        <Animated.View
          className="mt-10 flex-row items-center bg-card rounded-2xl px-6 py-4 border border-border"
          style={{
            opacity: fadeAnim,
            transform: [{ translateY: slideAnim }],
          }}
        >
          <View
            className="w-3 h-3 rounded-full mr-3"
            style={{ backgroundColor: COLORS.secondary }}
          />
          <Text className="text-sm text-textSecondary">
            Hệ thống đang{' '}
            <Text style={{ color: COLORS.secondary, fontWeight: '600' }}>trực tuyến</Text>
          </Text>
        </Animated.View>

      </View>
    </SafeAreaView>
  );
}
