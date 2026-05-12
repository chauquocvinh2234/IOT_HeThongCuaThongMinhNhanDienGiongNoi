import React from 'react';
import {
  View,
  Text,
  ScrollView,
  TouchableOpacity,
  Alert,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { SafeAreaView } from 'react-native-safe-area-context';
import { useAuth } from '../contexts/AuthContext';

const COLORS = {
  primary: '#6C63FF',
  background: '#0F0F1A',
  card: '#1A1A2E',
  cardLight: '#222240',
  text: '#FFFFFF',
  textSecondary: '#8A8AA3',
  border: '#2A2A4A',
  accent: '#FF6B8A',
};

export default function SettingsScreen({ navigation }) {
  const { user, logout } = useAuth();

  const handleLogout = () => {
    Alert.alert(
      'Xác nhận đăng xuất',
      'Bạn có chắc chắn muốn đăng xuất?',
      [
        { text: 'Hủy', style: 'cancel' },
        {
          text: 'Đăng xuất',
          style: 'destructive',
          onPress: () => {
            logout();
            navigation.replace('Login');
          },
        },
      ]
    );
  };

  return (
    <SafeAreaView className="flex-1 bg-background">
      <ScrollView
        className="flex-1"
        contentContainerStyle={{ paddingHorizontal: 20, paddingTop: 10 }}
        showsVerticalScrollIndicator={false}
      >
        <Text className="text-[26px] font-bold text-text mb-6 tracking-wide">Cài đặt</Text>

        {/* Thông tin người dùng */}
        <View className="bg-card rounded-[18px] border border-border overflow-hidden mb-6">
          {/* Header */}
          <View className="flex-row items-center p-5">
            <View
              className="w-16 h-16 rounded-2xl justify-center items-center mr-4"
              style={{ backgroundColor: COLORS.primary + '20' }}
            >
              <Ionicons name="person" size={32} color={COLORS.primary} />
            </View>
            <View className="flex-1">
              <Text className="text-lg font-bold text-text mb-1">
                {user?.fullname || 'Người dùng'}
              </Text>
              <Text className="text-sm text-textSecondary">
                @{user?.username || 'unknown'}
              </Text>
            </View>
          </View>

          {/* Divider */}
          <View className="h-[1px] bg-border mx-5" />

          {/* Info Rows */}
          <TouchableOpacity
            className="flex-row items-center p-4 mx-1"
            activeOpacity={0.7}
            onPress={() => {
              navigation.navigate('ChangeInformation');
            }}
          >
            <View
              className="w-10 h-10 rounded-xl justify-center items-center mr-3"
              style={{ backgroundColor: COLORS.primary + '15' }}
            >
              <Ionicons name="create-outline" size={20} color={COLORS.primary} />
            </View>
            <View className="flex-1">
              <Text className="text-[15px] font-semibold text-text mb-0.5">
                Chỉnh sửa thông tin
              </Text>
              <Text className="text-xs text-textSecondary">
                Cập nhật họ tên, giọng nói
              </Text>
            </View>
            <Ionicons name="chevron-forward" size={18} color={COLORS.textSecondary} />
          </TouchableOpacity>
        </View>

        {/* Logout Button */}
        <TouchableOpacity
          className="flex-row items-center justify-center bg-accent/15 rounded-[14px] p-4 border border-accent/30"
          activeOpacity={0.7}
          onPress={handleLogout}
        >
          <Ionicons name="log-out-outline" size={20} color={COLORS.accent} />
          <Text className="text-[15px] font-semibold text-accent ml-2">Đăng xuất</Text>
        </TouchableOpacity>

        <View className="h-5" />
      </ScrollView>
    </SafeAreaView>
  );
}
